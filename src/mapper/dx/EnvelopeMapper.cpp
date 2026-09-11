#include "mapper/dx/EnvelopeMapper.h"
#include "model/dx/ConversionContext.h"

#include <algorithm>
#include <cstddef>

void EnvelopeMapper::map(ConversionContext& context) {
    for (int group = 0; group < 2; ++group) {
        int dominant = -1;
        for (int slot = group * 2; slot < group * 2 + 2; ++slot) {
            const int dx = context.slotToDx[static_cast<std::size_t>(slot)];
            if (dx < 0) {
                continue;
            }
            if (dominant < 0 || context.operators[static_cast<std::size_t>(dx)].outputLevel > context.operators[static_cast<std::size_t>(dominant)].outputLevel) {
                dominant = dx;
            }
        }
        if (dominant < 0) {
            continue;
        }

        const OperatorView& view = context.operators[static_cast<std::size_t>(dominant)];
        const int attack = std::clamp(((99 - view.rates[0]) * 127 + 49) / 99, 0, 127);
        const int slowestDecayRate = std::min(view.rates[1], view.rates[2]);
        const int decay = std::clamp(((99 - slowestDecayRate) * 127 + 49) / 99, 0, 127);
        const int endLevel = std::clamp((view.levels[2] * 127 + 49) / 99, 0, 127);
        const bool triggered = view.levels[2] == 0;
        DigitoneEnvelope envelope = group == 0 ? context.target.envelopeA() : context.target.envelopeB();
        envelope.setAttack(attack);
        envelope.setDecay(decay);
        envelope.setEndLevel(endLevel);
        envelope.setDelay(0);
        envelope.setTriggered(triggered);
        envelope.setReset(true);
        if (group == 0) {
            context.target.setEnvelopeA(envelope);
        } else {
            context.target.setEnvelopeB(envelope);
        }
    }
}
