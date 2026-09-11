#include "mapper/dx/AlgorithmMapper.h"
#include "model/dx/ConversionContext.h"
#include "model/digitone/DigitoneTopology.h"
#include "model/dx/DxTopology.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

void AlgorithmMapper::map(ConversionContext& context) {
    const DxTopology dxTopology(context.dxAlgorithm);
    int carrierCount = 0;
    for (int op = 0; op < 6; ++op) {
        if (context.selected[static_cast<std::size_t>(op)] && context.operators[static_cast<std::size_t>(op)].carrier) {
            ++carrierCount;
        }
    }

    // Normalise the surviving DX graph: edges go from an earlier operator to a
    // later operator sharing a bus, which mirrors MSFA render order.
    std::array<std::array<bool, 6>, 6> edge{};
    for (auto& row : edge) {
        row.fill(false);
    }
    for (int j = 0; j < 6; ++j) {
        if (!context.selected[static_cast<std::size_t>(j)]) {
            continue;
        }
        const int outbus = dxTopology.getOutputBus(j);
        if (outbus == 0) {
            continue;
        }
        for (int k = j + 1; k < 6; ++k) {
            if (!context.selected[static_cast<std::size_t>(k)]) {
                continue;
            }
            if (dxTopology.getInputBus(k) == outbus) {
                edge[static_cast<std::size_t>(j)][static_cast<std::size_t>(k)] = true;
            }
        }
    }

    std::array<int, 6> depth{{0, 0, 0, 0, 0, 0}};
    for (int k = 0; k < 6; ++k) {
        if (!context.selected[static_cast<std::size_t>(k)]) {
            continue;
        }
        int best = 1;
        for (int j = 0; j < k; ++j) {
            if (edge[static_cast<std::size_t>(j)][static_cast<std::size_t>(k)]) {
                best = std::max(best, depth[static_cast<std::size_t>(j)] + 1);
            }
        }
        depth[static_cast<std::size_t>(k)] = best;
    }

    int maxDepth = 1;
    int maxFanIn = 0;
    for (int op = 0; op < 6; ++op) {
        if (!context.selected[static_cast<std::size_t>(op)] || !context.operators[static_cast<std::size_t>(op)].carrier) {
            continue;
        }
        maxDepth = std::max(maxDepth, depth[static_cast<std::size_t>(op)]);
        int fanIn = 0;
        for (int j = 0; j < 6; ++j) {
            if (edge[static_cast<std::size_t>(j)][static_cast<std::size_t>(op)]) {
                ++fanIn;
            }
        }
        maxFanIn = std::max(maxFanIn, fanIn);
    }

    if (carrierCount <= 1) {
        context.dnAlgorithm = maxFanIn >= 3 ? 8 : 1;
    } else if (carrierCount == 2) {
        context.dnAlgorithm = maxFanIn >= 2 ? 7 : 3;
    } else {
        context.dnAlgorithm = 4;
        if (carrierCount > 3) {
            context.report.warnings.push_back("Reduced " + std::to_string(carrierCount) + " DX carriers to 3 Digitone carriers");
        }
    }

    context.report.selectedAlgorithm = context.dnAlgorithm;
    context.target.setAlgorithm(context.dnAlgorithm);

    const DigitoneTopology dnTopology(context.dnAlgorithm);
    const std::vector<int> carrierSlots = dnTopology.getCarrierSlotsByWeight();
    const std::vector<int> modulatorPriority = dnTopology.getModulatorPriority();

    std::vector<int> survivingCarriers;
    std::vector<int> survivingModulators;
    for (int op = 0; op < 6; ++op) {
        if (!context.selected[static_cast<std::size_t>(op)]) {
            continue;
        }
        if (context.operators[static_cast<std::size_t>(op)].carrier) {
            survivingCarriers.push_back(op);
        } else {
            survivingModulators.push_back(op);
        }
    }
    const auto byScore = [&context](int a, int b) {
        const double scoreA = context.operators[static_cast<std::size_t>(a)].score;
        const double scoreB = context.operators[static_cast<std::size_t>(b)].score;
        if (scoreA != scoreB) {
            return scoreA > scoreB;
        }
        return a < b;
    };
    std::sort(survivingCarriers.begin(), survivingCarriers.end(), byScore);
    std::sort(survivingModulators.begin(), survivingModulators.end(), byScore);

    std::size_t carrierCursor = 0;
    std::size_t modulatorCursor = 0;
    context.slotToDx.fill(-1);
    for (int slot : carrierSlots) {
        if (carrierCursor < survivingCarriers.size()) {
            context.slotToDx[static_cast<std::size_t>(slot)] = survivingCarriers[carrierCursor++];
        } else if (modulatorCursor < survivingModulators.size()) {
            const int dx = survivingModulators[modulatorCursor++];
            context.slotToDx[static_cast<std::size_t>(slot)] = dx;
            context.report.warnings.push_back(DxTopology::getOpLabel(dx) + " (modulator) placed as Digitone carrier");
        }
    }
    for (int slot : modulatorPriority) {
        if (modulatorCursor < survivingModulators.size()) {
            context.slotToDx[static_cast<std::size_t>(slot)] = survivingModulators[modulatorCursor++];
        } else if (carrierCursor < survivingCarriers.size()) {
            const int dx = survivingCarriers[carrierCursor++];
            context.slotToDx[static_cast<std::size_t>(slot)] = dx;
            context.report.warnings.push_back(DxTopology::getOpLabel(dx) + " (carrier) placed as Digitone modulator");
        }
    }
}
