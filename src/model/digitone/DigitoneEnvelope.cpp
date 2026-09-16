#include "model/digitone/DigitoneEnvelope.h"

#include <stdexcept>
#include <string>

DigitoneEnvelope::DigitoneEnvelope(
    int attack,
    int decay,
    int endLevel,
    int level,
    int delay,
    bool triggered,
    bool reset
) {
    setAttack(attack);
    setDecay(decay);
    setEndLevel(endLevel);
    setLevel(level);
    setDelay(delay);
    setTriggered(triggered);
    setReset(reset);
}

int DigitoneEnvelope::getAttack() const {
    return attackValue;
}

int DigitoneEnvelope::getDecay() const {
    return decayValue;
}

int DigitoneEnvelope::getEndLevel() const {
    return endLevelValue;
}

int DigitoneEnvelope::getLevel() const {
    return levelValue;
}

int DigitoneEnvelope::getDelay() const {
    return delayValue;
}

bool DigitoneEnvelope::getTriggered() const {
    return triggeredValue;
}

bool DigitoneEnvelope::getReset() const {
    return resetValue;
}

void DigitoneEnvelope::setAttack(int value) {
    validateParameter("attack", value);
    attackValue = value;
}

void DigitoneEnvelope::setDecay(int value) {
    validateParameter("decay", value);
    decayValue = value;
}

void DigitoneEnvelope::setEndLevel(int value) {
    validateParameter("endLevel", value);
    endLevelValue = value;
}

void DigitoneEnvelope::setLevel(int value) {
    validateParameter("level", value);
    levelValue = value;
}

void DigitoneEnvelope::setDelay(int value) {
    validateParameter("delay", value);
    delayValue = value;
}

void DigitoneEnvelope::setTriggered(bool value) {
    triggeredValue = value;
}

void DigitoneEnvelope::setReset(bool value) {
    resetValue = value;
}

void DigitoneEnvelope::validateParameter(const char* name, int value) {
    if (value < parameterMinimum || value > parameterMaximum) {
        throw std::out_of_range(
            std::string(name) + " must be between 0 and 127"
        );
    }
}
