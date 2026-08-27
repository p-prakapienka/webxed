#pragma once

#include "model/digitone/DigitoneEnvelope.h"

#include <cstddef>

class DigitoneEnvelopeState {
public:
    explicit DigitoneEnvelopeState(double sampleRate);

    void load(const DigitoneEnvelope& envelope);
    void noteOn();
    void noteOff();
    double renderSample();

private:
    enum class Stage { idle, delay, attack, sustain, decay, end };

    static std::size_t durationSamples(int parameter, double sampleRate, double maximumSeconds);
    void startAttack();
    void startDecay();
    void advanceStage(double targetLevel);

    double sampleRate;
    DigitoneEnvelope envelope;
    Stage stage = Stage::idle;
    double value = 0.0;
    double stageStartValue = 0.0;
    std::size_t stagePosition = 0;
    std::size_t stageLength = 0;
};
