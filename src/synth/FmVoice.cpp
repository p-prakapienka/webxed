#include "synth/FmVoice.h"

#include <cmath>
#include <numbers>

namespace {
constexpr double twoPi = 2.0 * std::numbers::pi;
constexpr double modulatorRatio = 2.0;
constexpr double modulationIndex = 2.5;
}

FmVoice::FmVoice(double sampleRate) : sampleRate(sampleRate) {}

void FmVoice::noteOn(int midiNote, double velocity) {
    carrierFrequency = midiNoteToFrequency(midiNote);
    this->velocity = velocity;
    active = true;
}

void FmVoice::noteOff() {
    active = false;
}

double FmVoice::renderSample() {
    if (!active) {
        return 0.0;
    }

    const double carrierIncrement = twoPi * carrierFrequency / sampleRate;
    const double modulatorIncrement = carrierIncrement * modulatorRatio;
    const double modulation = std::sin(modulatorPhase) * modulationIndex;
    const double sample = std::sin(carrierPhase + modulation) * velocity * 0.2;

    carrierPhase = advancePhase(carrierPhase, carrierIncrement);
    modulatorPhase = advancePhase(modulatorPhase, modulatorIncrement);

    return sample;
}

double FmVoice::midiNoteToFrequency(int midiNote) {
    return 440.0 * std::pow(2.0, (static_cast<double>(midiNote) - 69.0) / 12.0);
}

double FmVoice::advancePhase(double phase, double increment) {
    phase += increment;
    return phase >= twoPi ? phase - twoPi : phase;
}
