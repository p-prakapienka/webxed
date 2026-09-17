#include "mapper/dx/LevelMapper.h"
#include "model/dx/ConversionContext.h"
#include "model/digitone/DigitoneTopology.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

void LevelMapper::map(ConversionContext& context) {
    const DigitoneTopology topology(context.dnAlgorithm);
    const auto& parents = topology.getParents();
    const auto groupOf = [](int slot) {
        return slot < 2 ? 0 : 1;
    };

    std::array<int, 2> groupLevel{{0, 0}};
    for (int slot = 0; slot < 4; ++slot) {
        const int dx = context.slotToDx[static_cast<std::size_t>(slot)];
        if (dx < 0) {
            continue;
        }
        const int group = groupOf(slot);
        groupLevel[static_cast<std::size_t>(group)] = std::max(groupLevel[static_cast<std::size_t>(group)], context.operators[static_cast<std::size_t>(dx)].outputLevel);
    }

    DigitoneEnvelope envelopeA = context.target.getEnvelopeA();
    DigitoneEnvelope envelopeB = context.target.getEnvelopeB();
    envelopeA.setLevel(std::clamp((groupLevel[0] * 127 + 49) / 99, 0, 127));
    envelopeB.setLevel(std::clamp((groupLevel[1] * 127 + 49) / 99, 0, 127));
    context.target.setEnvelopeA(envelopeA);
    context.target.setEnvelopeB(envelopeB);

    double weightA = 0.0;
    double weightB = 0.0;
    for (int slot = 0; slot < 4; ++slot) {
        const int dx = context.slotToDx[static_cast<std::size_t>(slot)];
        if (dx < 0) {
            continue;
        }
        const int parent = parents[static_cast<std::size_t>(slot)];
        const bool isCarrierSlot = parent < 0 || parent == slot;
        if (!isCarrierSlot) {
            continue;
        }
        if (groupOf(slot) == 0) {
            weightA += context.operators[static_cast<std::size_t>(dx)].outputLevel;
        } else {
            weightB += context.operators[static_cast<std::size_t>(dx)].outputLevel;
        }
    }

    if (weightA + weightB <= 0.0) {
        context.target.setMix(0);
    } else {
        const double fraction = weightA / (weightA + weightB);
        context.target.setMix(std::clamp(static_cast<int>(std::lround(127.0 * fraction - 64.0)), DigitonePatch::mixMinimum, DigitonePatch::mixMaximum));
    }
}
