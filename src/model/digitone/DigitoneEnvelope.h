#pragma once

class DigitoneEnvelope {
public:
    static constexpr int parameterMinimum = 0;
    static constexpr int parameterMaximum = 127;

    DigitoneEnvelope() = default;
    DigitoneEnvelope(
        int attack,
        int decay,
        int endLevel,
        int level,
        int delay,
        bool triggered,
        bool reset
    );

    int attack() const;
    int decay() const;
    int endLevel() const;
    int level() const;
    int delay() const;
    bool triggered() const;
    bool reset() const;

    void setAttack(int value);
    void setDecay(int value);
    void setEndLevel(int value);
    void setLevel(int value);
    void setDelay(int value);
    void setTriggered(bool value);
    void setReset(bool value);

    bool operator==(const DigitoneEnvelope&) const = default;

private:
    static void validateParameter(const char* name, int value);

    int attackValue = 0;
    int decayValue = 0;
    int endLevelValue = 0;
    int levelValue = 0;
    int delayValue = 0;
    bool triggeredValue = false;
    bool resetValue = true;
};
