#include "synth/digitone/DigitoneEngine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace {
constexpr double pi = 3.1415926535897932384626433832795;
constexpr double twoPi = 2.0 * pi;
}

DigitoneEngine::DigitoneEngine(double sampleRateValue)
    : sampleRate(sampleRateValue),
      operators{DigitoneOperator(sampleRateValue), DigitoneOperator(sampleRateValue),
                DigitoneOperator(sampleRateValue), DigitoneOperator(sampleRateValue)},
      envelopeA(sampleRateValue), envelopeB(sampleRateValue) {
    if (!(sampleRate > 0.0)) {
        throw std::invalid_argument("sample rate must be positive");
    }
    loadPatch(patch);
}

void DigitoneEngine::loadPatch(const DigitonePatch& value) {
    patch = value;
    envelopeA.load(patch.envelopeA());
    envelopeB.load(patch.envelopeB());
    configureOperators(midiNote);
    active = false;
}

void DigitoneEngine::noteOn(int note, double noteVelocity) {
    midiNote = std::clamp(note, 0, 127);
    velocity = std::clamp(noteVelocity, 0.0, 1.0);
    configureOperators(midiNote);
    for (auto& op : operators) {
        op.reset();
    }
    envelopeA.noteOn();
    envelopeB.noteOn();
    active = true;
}

void DigitoneEngine::noteOff() {
    envelopeA.noteOff();
    envelopeB.noteOff();
    if (!patch.envelopeA().triggered() && !patch.envelopeB().triggered()) {
        active = false;
    }
}

double DigitoneEngine::renderSample() {
    if (!active) {
        return 0.0;
    }

    const double ampA = envelopeA.renderSample();
    const double ampB = envelopeB.renderSample();
    const auto parents = parentsFor(patch.algorithm());
    OperatorArray modulation{};
    OperatorArray output{};

    for (int index = operatorCount - 1; index >= 0; --index) {
        output[index] = renderOperator(index, modulation);
        const int parent = parents[static_cast<std::size_t>(index)];
        if (parent >= 0) {
            modulation[parent] += output[index];
        }
    }

    const double mix = static_cast<double>(patch.mix() - DigitonePatch::mixMinimum)
        / static_cast<double>(DigitonePatch::mixMaximum - DigitonePatch::mixMinimum);
    const double carrierLevel = (output[0] * ampA + output[1] * ampA * mix + output[2] * ampB * (1.0 - mix) + output[3] * ampB * 0.25);
    const double amplitude = 0.2 + 0.8 * velocity;
    const double result = std::clamp(carrierLevel * amplitude, -1.0, 1.0);

    if (ampA <= 0.000001 && ampB <= 0.000001 && !patch.envelopeA().triggered() && !patch.envelopeB().triggered()) {
        active = false;
    }
    return result;
}

double DigitoneEngine::midiFrequency(int note) {
    return 440.0 * std::pow(2.0, (static_cast<double>(note) - 69.0) / 12.0);
}

const std::array<int, DigitoneEngine::operatorCount>& DigitoneEngine::parentsFor(int algorithm) {
    static constexpr std::array<std::array<int, operatorCount>, 8> algorithms = {{
        {{1, 2, 3, -1}},
        {{1, 2, 3, -1}},
        {{1, 2, -1, -1}},
        {{-1, 2, -1, -1}},
        {{1, -1, 2, -1}},
        {{-1, 2, 2, -1}},
        {{1, -1, 1, -1}},
        {{-1, 0, 0, 0}}
    }};
    const int index = std::clamp(algorithm, 1, 8) - 1;
    return algorithms[static_cast<std::size_t>(index)];
}

double DigitoneEngine::renderOperator(int index, const OperatorArray& phaseModulation) {
    double ratio = 1.0;
    switch (index) {
    case 0:
        ratio = patch.ratioC();
        break;
    case 1:
        ratio = patch.ratioA();
        break;
    case 2:
        ratio = patch.ratioB1();
        break;
    case 3:
        ratio = patch.ratioB2();
        break;
    default:
        break;
    }

    operators[static_cast<std::size_t>(index)].setFrequency(midiFrequency(midiNote) * ratio);
    operators[static_cast<std::size_t>(index)].setDetuneCents(static_cast<double>(patch.detune()) * 0.8);
    operators[static_cast<std::size_t>(index)].setFeedback(index == 0 ? static_cast<double>(patch.feedback()) / 127.0 : 0.0);
    const double modulationIndex = std::clamp(phaseModulation[static_cast<std::size_t>(index)] * 2.5, -twoPi, twoPi);
    return operators[static_cast<std::size_t>(index)].render(modulationIndex, patch.harmonic());
}

void DigitoneEngine::configureOperators(int note) {
    midiNote = std::clamp(note, 0, 127);
    for (int index = 0; index < operatorCount; ++index) {
        double ratio = 1.0;
        switch (index) {
        case 0: ratio = patch.ratioC(); break;
        case 1: ratio = patch.ratioA(); break;
        case 2: ratio = patch.ratioB1(); break;
        case 3: ratio = patch.ratioB2(); break;
        default: break;
        }
        operators[static_cast<std::size_t>(index)].setFrequency(midiFrequency(midiNote) * ratio);
        operators[static_cast<std::size_t>(index)].setDetuneCents(static_cast<double>(patch.detune()) * 0.8);
        operators[static_cast<std::size_t>(index)].setFeedback(index == 0 ? static_cast<double>(patch.feedback()) / 127.0 : 0.0);
    }
}
