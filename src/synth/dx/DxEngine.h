#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "env.h"
#include "fm_core.h"
#include "model/DxPatch.h"

class DxEngine {
public:
    explicit DxEngine(double sampleRate);

    void loadPatch(const DxPatch& patch);
    void noteOn(int midiNote, double velocity);
    void noteOff();
    double renderSample();

private:
    void configureOperator(std::size_t operatorIndex, int midiNote);
    void renderBlock();
    double operatorFrequency(std::size_t operatorIndex, double noteFrequency) const;
    int32_t frequencyToPhaseIncrement(double frequency) const;

    double sampleRate;
    double velocity = 0.0;
    bool active = false;
    int algorithm = 31;
    int feedbackShift = 16;
    DxPatch patch;
    FmCore core;
    std::array<Env, 6> envelopes{};
    std::array<FmOpParams, 6> operators{};
    std::array<int32_t, 2> feedbackBuffer{};
    std::array<int32_t, 64> outputBuffer{};
    std::size_t outputIndex = outputBuffer.size();
};
