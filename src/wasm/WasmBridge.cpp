#include "synth/dx/DxEngine.h"

extern "C" {

DxEngine* createSynth(double sampleRate) {
    return new DxEngine(sampleRate);
}

void destroySynth(DxEngine* synth) {
    delete synth;
}

void noteOn(DxEngine* synth, int midiNote, double velocity) {
    if (synth != nullptr) {
        synth->noteOn(midiNote, velocity);
    }
}

void noteOff(DxEngine* synth) {
    if (synth != nullptr) {
        synth->noteOff();
    }
}

double renderSample(DxEngine* synth) {
    return synth != nullptr ? synth->renderSample() : 0.0;
}

}

int main() {
    return 0;
}
