#include "mapper/dx/OperatorReducer.h"
#include "model/dx/ConversionContext.h"
#include "model/dx/DxTopology.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

void OperatorReducer::reduce(ConversionContext& context, const DxPatch& source) {
    analyze(context, source);
    select(context);
}

double OperatorReducer::idealRatio(const OperatorView& op) {
    if (op.mode == 0) {
        const double coarseRatio = op.coarse == 0 ? 0.5 : static_cast<double>(op.coarse);
        return coarseRatio * (1.0 + static_cast<double>(op.fine) / 100.0);
    }
    const double fixedFrequency = std::pow(10.0, static_cast<double>((op.coarse & 3) * 100 + op.fine) / 100.0);
    return fixedFrequency / 440.0;
}

void OperatorReducer::analyze(ConversionContext& context, const DxPatch& source) {
    context.sourcePatch = &source;
    context.sourceBytes = source.data();
    context.dxAlgorithm = context.sourceBytes[134] & 31;
    context.dxFeedback = context.sourceBytes[135] & 7;
    context.feedbackOp = -1;
    const DxTopology topology(context.dxAlgorithm);

    for (int op = 0; op < 6; ++op) {
        const std::size_t offset = static_cast<std::size_t>(op) * 21;
        OperatorView& view = context.operators[static_cast<std::size_t>(op)];
        view.index = op;
        for (int i = 0; i < 4; ++i) {
            view.rates[static_cast<std::size_t>(i)] = context.sourceBytes[offset + static_cast<std::size_t>(i)];
            view.levels[static_cast<std::size_t>(i)] = context.sourceBytes[offset + 4 + static_cast<std::size_t>(i)];
        }
        view.outputLevel = context.sourceBytes[offset + 16];
        view.mode = context.sourceBytes[offset + 17];
        view.coarse = context.sourceBytes[offset + 18] & 31;
        view.fine = context.sourceBytes[offset + 19];
        view.detune = static_cast<int>(context.sourceBytes[offset + 20]) - 7;
        view.velocitySensitivity = context.sourceBytes[offset + 15] & 7;
        view.carrier = topology.isCarrier(op);
        view.feedback = topology.isFeedbackOperator(op);
        view.idealRatio = idealRatio(view);
        view.score = 0.0;
        if (view.feedback) {
            context.feedbackOp = op;
        }
    }
}

void OperatorReducer::select(ConversionContext& context) {    const DxTopology topology(context.dxAlgorithm);
    // Score modulators by contribution so the two lowest-value operators can be
    // discarded. Inputs follow the plan: output level, topology role, carrier
    // distance, feedback participation, envelope energy, velocity sensitivity.
    for (auto& view : context.operators) {
        double score = static_cast<double>(view.outputLevel);
        if (view.carrier) {
            score += 60.0;
        }
        if (view.feedback && context.dxFeedback > 0) {
            score += 40.0;
        }
        const double averageLevel = (static_cast<double>(view.levels[0] + view.levels[1] + view.levels[2]) / 3.0) / 99.0;
        score += static_cast<double>(view.outputLevel) * averageLevel * 0.25;
        score += static_cast<double>(view.velocitySensitivity) * 2.0;
        if (!view.carrier) {
            const int outbus = topology.getOutputBus(view.index);
            if (outbus != 0) {
                for (const auto& other : context.operators) {
                    if (!other.carrier) {
                        continue;
                    }
                    if (topology.getInputBus(other.index) == outbus && other.index > view.index) {
                        score += 10.0;
                        break;
                    }
                }
            }
        }
        if (view.mode == 1) {
            score -= 10.0;
        }
        view.score = score;
    }

    std::array<int, 6> order{{0, 1, 2, 3, 4, 5}};
    std::sort(order.begin(), order.end(), [&context](int a, int b) {
        const double scoreA = context.operators[static_cast<std::size_t>(a)].score;
        const double scoreB = context.operators[static_cast<std::size_t>(b)].score;
        if (scoreA != scoreB) {
            return scoreA > scoreB;
        }
        return a < b;
    });

    std::array<bool, 6> kept{};
    kept.fill(false);
    for (int i = 0; i < 4; ++i) {
        kept[static_cast<std::size_t>(order[static_cast<std::size_t>(i)])] = true;
    }

    const auto hasCarrier = [&kept, &context] {
        for (int op = 0; op < 6; ++op) {
            if (kept[static_cast<std::size_t>(op)] && context.operators[static_cast<std::size_t>(op)].carrier) {
                return true;
            }
        }
        return false;
    };

    if (!hasCarrier()) {
        int replacement = -1;
        for (int op = 0; op < 6; ++op) {
            if (!kept[static_cast<std::size_t>(op)] && context.operators[static_cast<std::size_t>(op)].carrier) {
                if (replacement < 0 || context.operators[static_cast<std::size_t>(op)].score > context.operators[static_cast<std::size_t>(replacement)].score) {
                    replacement = op;
                }
            }
        }
        int evicted = -1;
        for (int op = 0; op < 6; ++op) {
            if (kept[static_cast<std::size_t>(op)]) {
                if (evicted < 0 || context.operators[static_cast<std::size_t>(op)].score < context.operators[static_cast<std::size_t>(evicted)].score) {
                    evicted = op;
                }
            }
        }
        if (replacement >= 0 && evicted >= 0) {
            kept[static_cast<std::size_t>(evicted)] = false;
            kept[static_cast<std::size_t>(replacement)] = true;
        }
    }

    if (context.dxFeedback > 0 && context.feedbackOp >= 0 && !kept[static_cast<std::size_t>(context.feedbackOp)]) {
        int evicted = -1;
        for (int op = 0; op < 6; ++op) {
            if (kept[static_cast<std::size_t>(op)] && !context.operators[static_cast<std::size_t>(op)].carrier) {
                if (evicted < 0 || context.operators[static_cast<std::size_t>(op)].score < context.operators[static_cast<std::size_t>(evicted)].score) {
                    evicted = op;
                }
            }
        }
        if (evicted < 0) {
            for (int op = 0; op < 6; ++op) {
                if (kept[static_cast<std::size_t>(op)]) {
                    if (evicted < 0 || context.operators[static_cast<std::size_t>(op)].score < context.operators[static_cast<std::size_t>(evicted)].score) {
                        evicted = op;
                    }
                }
            }
        }
        if (evicted >= 0) {
            kept[static_cast<std::size_t>(evicted)] = false;
            kept[static_cast<std::size_t>(context.feedbackOp)] = true;
            context.report.warnings.push_back("Feedback operator " + DxTopology::getOpLabel(context.feedbackOp) + " preserved over " + DxTopology::getOpLabel(evicted));
        }
    }

    context.selected = kept;
    context.report.removedOperators.clear();
    for (int op = 0; op < 6; ++op) {
        if (!kept[static_cast<std::size_t>(op)]) {
            context.report.removedOperators.push_back(op);
            if (context.operators[static_cast<std::size_t>(op)].carrier) {
                context.report.warnings.push_back(DxTopology::getOpLabel(op) + " (carrier) discarded by operator reduction");
            } else {
                context.report.warnings.push_back(DxTopology::getOpLabel(op) + " (modulator) discarded by operator reduction");
            }
        }
    }
}
