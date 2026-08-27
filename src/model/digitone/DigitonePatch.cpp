#include "model/digitone/DigitonePatch.h"

#include <cmath>
#include <stdexcept>
#include <utility>

const std::string& DigitonePatch::name() const {
    return nameValue;
}

int DigitonePatch::algorithm() const {
    return algorithmValue;
}

double DigitonePatch::ratioC() const {
    return ratioCValue;
}

double DigitonePatch::ratioA() const {
    return ratioAValue;
}

double DigitonePatch::ratioB1() const {
    return ratioB1Value;
}

double DigitonePatch::ratioB2() const {
    return ratioB2Value;
}

double DigitonePatch::harmonic() const {
    return harmonicValue;
}

int DigitonePatch::detune() const {
    return detuneValue;
}

int DigitonePatch::feedback() const {
    return feedbackValue;
}

int DigitonePatch::mix() const {
    return mixValue;
}

const DigitoneEnvelope& DigitonePatch::envelopeA() const {
    return envelopeAValue;
}

const DigitoneEnvelope& DigitonePatch::envelopeB() const {
    return envelopeBValue;
}

void DigitonePatch::setName(std::string value) {
    nameValue = std::move(value);
}

void DigitonePatch::setAlgorithm(int value) {
    if (value < algorithmMinimum || value > algorithmMaximum) {
        throw std::out_of_range("algorithm must be between 1 and 8");
    }
    algorithmValue = value;
}

void DigitonePatch::setRatioC(double value) {
    validateRatio("ratioC", value);
    ratioCValue = value;
}

void DigitonePatch::setRatioA(double value) {
    validateRatio("ratioA", value);
    ratioAValue = value;
}

void DigitonePatch::setRatioB1(double value) {
    validateRatio("ratioB1", value);
    ratioB1Value = value;
}

void DigitonePatch::setRatioB2(double value) {
    validateRatio("ratioB2", value);
    ratioB2Value = value;
}

void DigitonePatch::setHarmonic(double value) {
    if (!std::isfinite(value) || value < harmonicMinimum || value > harmonicMaximum) {
        throw std::out_of_range("harmonic must be between -26 and 26");
    }
    harmonicValue = value;
}

void DigitonePatch::setDetune(int value) {
    validateParameter("detune", value);
    detuneValue = value;
}

void DigitonePatch::setFeedback(int value) {
    validateParameter("feedback", value);
    feedbackValue = value;
}

void DigitonePatch::setMix(int value) {
    if (value < mixMinimum || value > mixMaximum) {
        throw std::out_of_range("mix must be between -64 and 63");
    }
    mixValue = value;
}

void DigitonePatch::setEnvelopeA(DigitoneEnvelope value) {
    envelopeAValue = value;
}

void DigitonePatch::setEnvelopeB(DigitoneEnvelope value) {
    envelopeBValue = value;
}

void DigitonePatch::validateRatio(const char* name, double value) {
    if (!std::isfinite(value) || value < ratioMinimum || value > ratioMaximum) {
        throw std::out_of_range(
            std::string(name) + " must be between 0.25 and 16"
        );
    }
}

void DigitonePatch::validateParameter(const char* name, int value) {
    if (value < parameterMinimum || value > parameterMaximum) {
        throw std::out_of_range(
            std::string(name) + " must be between 0 and 127"
        );
    }
}
