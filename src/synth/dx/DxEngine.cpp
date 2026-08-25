#include "synth/dx/DxEngine.h"

#include <algorithm>
#include <cmath>

#include "exp2.h"
#include "sin.h"

namespace {
constexpr int operatorLevel = 14 << 24;
constexpr double outputScale = 1.0 / static_cast<double>(1 << 24);
}

DxEngine::DxEngine(double sampleRate) : sampleRate(sampleRate) {
    Exp2::init();
    Sin::init();

    for (auto& op : operators) {
        op.level_in = 0;
        op.gain_out = 0;
        op.freq = 0;
        op.phase = 0;
    }
}

void DxEngine::noteOn(int midiNote, double velocity) {
    const double frequency = 440.0 * std::pow(2.0, (static_cast<double>(midiNote) - 69.0) / 12.0);
    const int32_t baseIncrement = frequencyToPhaseIncrement(frequency);

    for (auto& op : operators) {
        op.phase = 0;
        op.freq = baseIncrement;
        op.level_in = 0;
        op.gain_out = 0;
    }

    // DX algorithm 32: six parallel carriers. Keep only operator 1 audible for
    // the initial MSFA integration test, which produces a clean DX sine voice.
    operators[0].level_in = operatorLevel;
    this->velocity = std::clamp(velocity, 0.0, 1.0);
    feedbackBuffer.fill(0);
    outputIndex = outputBuffer.size();
    active = true;
}

void DxEngine::noteOff() {
    active = false;
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

void DxEngine::renderBlock() {
    outputBuffer.fill(0);
    core.render(outputBuffer.data(), operators.data(), 31, feedbackBuffer.data(), 16);
    outputIndex = 0;
}

int32_t DxEngine::frequencyToPhaseIncrement(double frequency) const {
    return static_cast<int32_t>(std::llround(frequency * static_cast<double>(1 << 24) / sampleRate));
}
