#pragma once

#include "model/digitone/DigitonePatch.h"
#include "synth/digitone/DigitoneEnvelopeState.h"
#include "synth/digitone/DigitoneOperator.h"

#include <array>

class DigitoneEngine {
public:
    explicit DigitoneEngine(double sampleRate);

    void loadPatch(const DigitonePatch& patch);
    void noteOn(int midiNote, double velocity);
    void noteOff();
    double renderSample();

private:
    static constexpr int operatorCount = 4;
    using OperatorArray = std::array<double, operatorCount>;

    static double midiFrequency(int midiNote);
    static const std::array<int, operatorCount>& parentsFor(int algorithm);

    double renderOperator(int index, const OperatorArray& phaseModulation);
    void configureOperators(int midiNote);

    double sampleRate;
    DigitonePatch patch;
    std::array<DigitoneOperator, operatorCount> operators;
    DigitoneEnvelopeState envelopeA;
    DigitoneEnvelopeState envelopeB;
    int midiNote = 69;
    double velocity = 0.0;
    bool active = false;
};
