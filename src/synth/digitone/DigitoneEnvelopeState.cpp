#include "synth/digitone/DigitoneEnvelopeState.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>

DigitoneEnvelopeState::DigitoneEnvelopeState(double sampleRateValue) : sampleRate(sampleRateValue) {
    if (!(sampleRate > 0.0)) {
        throw std::invalid_argument("sample rate must be positive");
    }
}

void DigitoneEnvelopeState::load(const DigitoneEnvelope& value) {
    envelope = value;
    stage = Stage::idle;
    this->value = 0.0;
    stageStartValue = 0.0;
    stagePosition = 0;
    stageLength = 0;
}

void DigitoneEnvelopeState::update(const DigitoneEnvelope& value) {
    envelope = value;
}

void DigitoneEnvelopeState::noteOn() {
    if (envelope.getReset()) {
        value = 0.0;
    }
    stagePosition = 0;
    stageLength = durationSamples(envelope.getDelay(), sampleRate, 4.0);
    stageStartValue = value;
    stage = stageLength == 0 ? Stage::attack : Stage::delay;
    if (stage == Stage::attack) {
        startAttack();
    }
}

void DigitoneEnvelopeState::noteOff() {
    if (!envelope.getTriggered() && stage != Stage::idle && stage != Stage::end) {
        startDecay();
    }
}

double DigitoneEnvelopeState::renderSample() {
    switch (stage) {
    case Stage::idle:
        return 0.0;
    case Stage::delay:
        if (++stagePosition >= stageLength) {
            startAttack();
        }
        break;
    case Stage::attack:
        advanceStage(1.0);
        if (stagePosition >= stageLength) {
            if (envelope.getTriggered()) {
                startDecay();
            } else {
                stage = Stage::sustain;
                value = 1.0;
            }
        }
        break;
    case Stage::sustain:
        value = 1.0;
        break;
    case Stage::decay:
        advanceStage(static_cast<double>(envelope.getEndLevel()) / 127.0);
        if (stagePosition >= stageLength) {
            stage = Stage::end;
            value = static_cast<double>(envelope.getEndLevel()) / 127.0;
        }
        break;
    case Stage::end:
        value = static_cast<double>(envelope.getEndLevel()) / 127.0;
        break;
    }
    return value * static_cast<double>(envelope.getLevel()) / 127.0;
}

void DigitoneEnvelopeState::startAttack() {
    stage = Stage::attack;
    stageStartValue = value;
    stagePosition = 0;
    stageLength = durationSamples(envelope.getAttack(), sampleRate, 8.0);
    if (stageLength == 0) {
        value = 1.0;
        if (envelope.getTriggered()) {
            startDecay();
        } else {
            stage = Stage::sustain;
        }
    }
}

void DigitoneEnvelopeState::startDecay() {
    stage = Stage::decay;
    stageStartValue = value;
    stagePosition = 0;
    stageLength = durationSamples(envelope.getDecay(), sampleRate, 12.0);
    if (stageLength == 0) {
        value = static_cast<double>(envelope.getEndLevel()) / 127.0;
        stage = Stage::end;
    }
}

void DigitoneEnvelopeState::advanceStage(double targetLevel) {
    if (stageLength == 0) {
        value = targetLevel;
        return;
    }
    ++stagePosition;
    const double progress = std::min(1.0, static_cast<double>(stagePosition) / static_cast<double>(stageLength));
    value = stageStartValue + (targetLevel - stageStartValue) * progress;
}

std::size_t DigitoneEnvelopeState::durationSamples(int parameter, double sampleRateValue, double maximumSeconds) {
    if (parameter <= 0) {
        return 0;
    }
    const double normalised = static_cast<double>(parameter) / 127.0;
    const double seconds = maximumSeconds * normalised * normalised;
    return static_cast<std::size_t>(std::max(1.0, std::round(seconds * sampleRateValue)));
}
