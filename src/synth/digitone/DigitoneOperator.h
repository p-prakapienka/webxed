#pragma once

class DigitoneOperator {
public:
    explicit DigitoneOperator(double sampleRate);

    void setFrequency(double frequency);
    void setDetuneCents(double cents);
    void setFeedback(double amount);
    void reset();
    double render(double phaseModulation, double harmonicAmount);

private:
    double sampleRate;
    double frequency = 440.0;
    double detuneCents = 0.0;
    double feedback = 0.0;
    double phase = 0.0;
    double feedbackSample = 0.0;
};
