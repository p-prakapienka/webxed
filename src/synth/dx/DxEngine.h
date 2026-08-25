#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "fm_core.h"

class DxEngine {
public:
    explicit DxEngine(double sampleRate);

    void noteOn(int midiNote, double velocity);
    void noteOff();
    double renderSample();

private:
    void renderBlock();
    int32_t frequencyToPhaseIncrement(double frequency) const;

    double sampleRate;
    double velocity = 0.0;
    bool active = false;
    FmCore core;
    std::array<FmOpParams, 6> operators{};
    std::array<int32_t, 2> feedbackBuffer{};
    std::array<int32_t, 64> outputBuffer{};
    std::size_t outputIndex = outputBuffer.size();
};
