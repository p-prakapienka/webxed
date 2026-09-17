#include "wasm/WebxedSession.h"

#include <cstddef>
#include <cstdint>

extern "C" {

WebxedSession* createSynth(double sampleRate) {
    return new WebxedSession(sampleRate);
}

void destroySynth(WebxedSession* session) {
    delete session;
}

int loadSysex(WebxedSession* session, const uint8_t* data, int size) {
    return session != nullptr && data != nullptr && size > 0
        ? session->loadSysex(data, static_cast<std::size_t>(size))
        : -1;
}

int patchCount(WebxedSession* session) {
    return session != nullptr ? session->getPatchCount() : 0;
}

const char* patchName(WebxedSession* session, int index) {
    return session != nullptr ? session->getPatchName(index) : "";
}

int selectPatch(WebxedSession* session, int index) {
    return session != nullptr && session->selectPatch(index) ? 1 : 0;
}

int selectPreviewEngine(WebxedSession* session, int engineIndex) {
    return session != nullptr && session->selectPreviewEngine(engineIndex) ? 1 : 0;
}

int convert(WebxedSession* session) {
    return session != nullptr && session->convert() ? 1 : 0;
}

int applyDigitoneJson(WebxedSession* session, const char* json) {
    return session != nullptr && session->applyDigitoneJson(json) ? 1 : 0;
}

const char* conversionJson(WebxedSession* session) {
    return session != nullptr ? session->getConversionJson() : "{}";
}

void noteOn(WebxedSession* session, int midiNote, double velocity) {
    if (session != nullptr) {
        session->noteOn(midiNote, velocity);
    }
}

void noteOff(WebxedSession* session) {
    if (session != nullptr) {
        session->noteOff();
    }
}

double renderSample(WebxedSession* session) {
    return session != nullptr ? session->renderSample() : 0.0;
}

}

int main() {
    return 0;
}
