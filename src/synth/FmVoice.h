#pragma once

class FmVoice {
public:
    explicit FmVoice(double sampleRate);

    void noteOn(int midiNote, double velocity);
    void noteOff();
    double renderSample();

private:
    static double midiNoteToFrequency(int midiNote);
    static double advancePhase(double phase, double increment);

    double sampleRate;
    double carrierPhase = 0.0;
    double modulatorPhase = 0.0;
    double carrierFrequency = 440.0;
    double velocity = 0.0;
    bool active = false;
};
