#include "synth/digitone/DigitoneOperator.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr double pi = 3.1415926535897932384626433832795;
constexpr double twoPi = 2.0 * pi;
}

DigitoneOperator::DigitoneOperator(double sampleRateValue) : sampleRate(sampleRateValue) {}

void DigitoneOperator::setFrequency(double value) {
    frequency = std::clamp(value, 0.0, sampleRate * 0.45);
}

void DigitoneOperator::setDetuneCents(double cents) {
    detuneCents = std::clamp(cents, -100.0, 100.0);
}

void DigitoneOperator::setFeedback(double amount) {
    feedback = std::clamp(amount, 0.0, 1.0);
}

void DigitoneOperator::reset() {
    phase = 0.0;
    feedbackSample = 0.0;
}

double DigitoneOperator::render(double phaseModulation, double harmonicAmount) {
    const double detunedFrequency = frequency * std::pow(2.0, detuneCents / 1200.0);
    const double phaseInput = phaseModulation + feedbackSample * feedback * 2.0;
    const double harmonic = std::clamp(harmonicAmount, -26.0, 26.0) / 26.0;
    const double harmonicPhase = phase + phaseInput;
    const double sine = std::sin(harmonicPhase);
    const double bright = std::sin(harmonicPhase * (1.0 + 7.0 * std::abs(harmonic)));
    const double output = sine * (1.0 - 0.35 * std::abs(harmonic)) + bright * 0.35 * harmonic;

    feedbackSample = output;
    phase += twoPi * detunedFrequency / sampleRate;
    if (phase >= twoPi) {
        phase = std::fmod(phase, twoPi);
    }
    return output;
}
