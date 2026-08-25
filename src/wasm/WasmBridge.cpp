#include "synth/FmVoice.h"

extern "C" {

FmVoice* createSynth(double sampleRate) {
    return new FmVoice(sampleRate);
}

void destroySynth(FmVoice* synth) {
    delete synth;
}

void noteOn(FmVoice* synth, int midiNote, double velocity) {
    if (synth != nullptr) {
        synth->noteOn(midiNote, velocity);
    }
}

void noteOff(FmVoice* synth) {
    if (synth != nullptr) {
        synth->noteOff();
    }
}

double renderSample(FmVoice* synth) {
    return synth != nullptr ? synth->renderSample() : 0.0;
}

}

int main() {
    return 0;
}
