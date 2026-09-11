#include "mapper/dx/RatioMapper.h"
#include "model/dx/ConversionContext.h"
#include "model/digitone/DigitoneTopology.h"
#include "model/dx/DxTopology.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>

void RatioMapper::map(ConversionContext& context) {
    double totalDetune = 0.0;
    int detuneCount = 0;
    double maxError = 0.0;

    for (int slot = 0; slot < 4; ++slot) {
        const int dx = context.slotToDx[static_cast<std::size_t>(slot)];
        if (dx < 0) {
            continue;
        }
        const OperatorView& view = context.operators[static_cast<std::size_t>(dx)];
        double ideal = view.idealRatio;
        if (view.mode == 1) {
            context.report.warnings.push_back(DxTopology::getOpLabel(dx) + " uses fixed frequency; approximated as ratio " + std::to_string(ideal));
        }
        if (ideal < DigitonePatch::ratioMinimum || ideal > DigitonePatch::ratioMaximum) {
            context.report.warnings.push_back(DxTopology::getOpLabel(dx) + " ratio clamped into 0.25..16 range");
        }
        ideal = std::clamp(ideal, DigitonePatch::ratioMinimum, DigitonePatch::ratioMaximum);

        double error = 0.0;
        const double snapped = DigitoneTopology::snapToNearestRatio(ideal, error);
        maxError = std::max(maxError, error);
        if (error > 0.05) {
            context.report.warnings.push_back(DxTopology::getOpLabel(dx) + " ratio approximated (" + std::to_string(ideal) + " -> " + std::to_string(snapped) + ")");
        }

        switch (slot) {
        case 0:
            context.target.setRatioC(snapped);
            break;
        case 1:
            context.target.setRatioA(snapped);
            break;
        case 2:
            context.target.setRatioB1(snapped);
            break;
        case 3:
            context.target.setRatioB2(snapped);
            break;
        default:
            break;
        }

        totalDetune += static_cast<double>(view.detune);
        ++detuneCount;
    }

    context.report.ratioApproximationError = maxError;
    if (maxError > 0.25) {
        context.report.warnings.push_back("Ratio approximation error up to " + std::to_string(maxError));
    }

    if (detuneCount > 0) {
        const double average = totalDetune / static_cast<double>(detuneCount);
        context.target.setDetune(std::clamp(static_cast<int>(std::lround(std::abs(average) * 6.0)), 0, 127));
    } else {
        context.target.setDetune(0);
    }
}
