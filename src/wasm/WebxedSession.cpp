#include "wasm/WebxedSession.h"

#include <span>

WebxedSession::WebxedSession(double sampleRate) : engine(sampleRate), patches{DxPatch::initVoice()} {
    engine.loadPatch(patches.front());
}

int WebxedSession::loadSysex(const uint8_t* data, std::size_t size) {
    try {
        patches = parser.parse(std::span<const uint8_t>(data, size));
        selectedPatch = 0;
        engine.loadPatch(patches.front());
        return static_cast<int>(patches.size());
    } catch (...) {
        return -1;
    }
}

int WebxedSession::patchCount() const {
    return static_cast<int>(patches.size());
}

const char* WebxedSession::patchName(int index) {
    if (index < 0 || static_cast<std::size_t>(index) >= patches.size()) {
        nameBuffer.clear();
        return nameBuffer.c_str();
    }
    nameBuffer = patches[static_cast<std::size_t>(index)].name();
    return nameBuffer.c_str();
}

bool WebxedSession::selectPatch(int index) {
    if (index < 0 || static_cast<std::size_t>(index) >= patches.size()) {
        return false;
    }
    selectedPatch = static_cast<std::size_t>(index);
    engine.loadPatch(patches[selectedPatch]);
    return true;
}

void WebxedSession::noteOn(int midiNote, double velocity) {
    engine.noteOn(midiNote, velocity);
}

void WebxedSession::noteOff() {
    engine.noteOff();
}

double WebxedSession::renderSample() {
    return engine.renderSample();
}
