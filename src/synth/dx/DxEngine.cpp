#include "synth/dx/DxEngine.h"

#include <algorithm>
#include <cmath>

#include "exp2.h"
#include "sin.h"

namespace {
constexpr double outputScale = 1.0 / static_cast<double>(1 << 24);
}

DxEngine::DxEngine(double sampleRate)
    : sampleRate(sampleRate), patch(DxPatch::initVoice()) {
    Exp2::init();
    Sin::init();
    Env::init_sr(sampleRate);

    for (auto& op : operators) {
        op.level_in = 0;
        op.gain_out = 0;
        op.freq = 0;
        op.phase = 0;
    }

    loadPatch(patch);
}

void DxEngine::loadPatch(const DxPatch& patch) {
    this->patch = patch;
    const auto& values = this->patch.data();
    algorithm = values[134] & 31;
    const int feedback = values[135] & 7;
    feedbackShift = feedback != 0 ? 8 - feedback : 16;
}

void DxEngine::noteOn(int midiNote, double velocity) {
    for (std::size_t op = 0; op < operators.size(); ++op) {
        configureOperator(op, midiNote);
    }

    this->velocity = std::clamp(velocity, 0.0, 1.0);
    feedbackBuffer.fill(0);
    outputIndex = outputBuffer.size();
    active = true;
}

void DxEngine::noteOff() {
    for (auto& envelope : envelopes) {
        envelope.keydown(false);
    }
}

double DxEngine::renderSample() {
    if (!active) {
        return 0.0;
    }

    if (outputIndex >= outputBuffer.size()) {
        renderBlock();
    }

    return static_cast<double>(outputBuffer[outputIndex++]) * outputScale * velocity;
}

void DxEngine::configureOperator(std::size_t operatorIndex, int midiNote) {
    const auto& values = patch.data();
    const std::size_t offset = operatorIndex * 21;

    int rates[4];
    int levels[4];
    for (std::size_t index = 0; index < 4; ++index) {
        rates[index] = values[offset + index];
        levels[index] = values[offset + 4 + index];
    }

    const int scaledOutputLevel = Env::scaleoutlevel(values[offset + 16]) << 5;
    envelopes[operatorIndex].init(rates, levels, scaledOutputLevel, 0);
    envelopes[operatorIndex].keydown(true);

    const double noteFrequency = 440.0 * std::pow(2.0, (static_cast<double>(midiNote) - 69.0) / 12.0);
    operators[operatorIndex].phase = 0;
    operators[operatorIndex].freq = frequencyToPhaseIncrement(operatorFrequency(operatorIndex, noteFrequency));
    operators[operatorIndex].level_in = 0;
    operators[operatorIndex].gain_out = 0;
}

void DxEngine::renderBlock() {
    outputBuffer.fill(0);

    bool envelopeActive = false;
    for (std::size_t op = 0; op < operators.size(); ++op) {
        operators[op].level_in = envelopes[op].getsample();
        envelopeActive = envelopeActive || envelopes[op].isActive();
    }

    core.render(outputBuffer.data(), operators.data(), algorithm, feedbackBuffer.data(), feedbackShift);
    outputIndex = 0;
    active = envelopeActive;
}

double DxEngine::operatorFrequency(std::size_t operatorIndex, double noteFrequency) const {
    const auto& values = patch.data();
    const std::size_t offset = operatorIndex * 21;
    const int mode = values[offset + 17];
    const int coarse = values[offset + 18] & 31;
    const int fine = values[offset + 19];
    const int detune = static_cast<int>(values[offset + 20]) - 7;

    if (mode == 0) {
        const double coarseRatio = coarse == 0 ? 0.5 : static_cast<double>(coarse);
        const double fineRatio = 1.0 + static_cast<double>(fine) / 100.0;
        const double detuneRatio = std::pow(2.0, static_cast<double>(detune) / 1200.0);
        return noteFrequency * coarseRatio * fineRatio * detuneRatio;
    }

    const double fixedFrequency = std::pow(10.0, static_cast<double>((coarse & 3) * 100 + fine) / 100.0);
    return fixedFrequency * std::pow(2.0, static_cast<double>(std::max(detune, 0)) / 1200.0);
}

int32_t DxEngine::frequencyToPhaseIncrement(double frequency) const {
    return static_cast<int32_t>(std::llround(frequency * static_cast<double>(1 << 24) / sampleRate));
}
