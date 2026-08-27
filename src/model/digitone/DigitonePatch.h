#pragma once

#include "model/digitone/DigitoneEnvelope.h"

#include <string>

class DigitonePatch {
public:
    static constexpr int algorithmMinimum = 1;
    static constexpr int algorithmMaximum = 8;
    static constexpr double ratioMinimum = 0.25;
    static constexpr double ratioMaximum = 16.0;
    static constexpr double harmonicMinimum = -26.0;
    static constexpr double harmonicMaximum = 26.0;
    static constexpr int parameterMinimum = 0;
    static constexpr int parameterMaximum = 127;
    static constexpr int mixMinimum = -64;
    static constexpr int mixMaximum = 63;

    DigitonePatch() = default;

    const std::string& name() const;
    int algorithm() const;
    double ratioC() const;
    double ratioA() const;
    double ratioB1() const;
    double ratioB2() const;
    double harmonic() const;
    int detune() const;
    int feedback() const;
    int mix() const;
    const DigitoneEnvelope& envelopeA() const;
    const DigitoneEnvelope& envelopeB() const;

    void setName(std::string value);
    void setAlgorithm(int value);
    void setRatioC(double value);
    void setRatioA(double value);
    void setRatioB1(double value);
    void setRatioB2(double value);
    void setHarmonic(double value);
    void setDetune(int value);
    void setFeedback(int value);
    void setMix(int value);
    void setEnvelopeA(DigitoneEnvelope value);
    void setEnvelopeB(DigitoneEnvelope value);

    bool operator==(const DigitonePatch&) const = default;

private:
    static void validateRatio(const char* name, double value);
    static void validateParameter(const char* name, int value);

    std::string nameValue = "INIT";
    int algorithmValue = 1;
    double ratioCValue = 1.0;
    double ratioAValue = 1.0;
    double ratioB1Value = 1.0;
    double ratioB2Value = 1.0;
    double harmonicValue = 0.0;
    int detuneValue = 0;
    int feedbackValue = 0;
    int mixValue = 0;
    DigitoneEnvelope envelopeAValue;
    DigitoneEnvelope envelopeBValue;
};
