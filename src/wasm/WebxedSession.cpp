#include "wasm/WebxedSession.h"

#include <span>

namespace {
DigitonePatch initDigitonePatch() {
    DigitonePatch patch;
    patch.setName("INIT DN");
    patch.setAlgorithm(1);
    patch.setRatioC(1.0);
    patch.setRatioA(2.0);
    patch.setRatioB1(3.0);
    patch.setRatioB2(6.0);
    patch.setHarmonic(0.0);
    patch.setDetune(0);
    patch.setFeedback(24);
    patch.setMix(0);
    patch.setEnvelopeA(DigitoneEnvelope(4, 60, 70, 127, 0, false, true));
    patch.setEnvelopeB(DigitoneEnvelope(0, 48, 30, 110, 0, false, true));
    return patch;
}
}

WebxedSession::WebxedSession(double sampleRate)
    : dxEngine(sampleRate), digitoneEngine(sampleRate), patches{DxPatch::initVoice()} {
    dxEngine.loadPatch(patches.front());
    digitoneEngine.loadPatch(initDigitonePatch());
}

int WebxedSession::loadSysex(const uint8_t* data, std::size_t size) {
    try {
        patches = parser.parse(std::span<const uint8_t>(data, size));
        selectedPatch = 0;
        dxEngine.loadPatch(patches.front());
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
    dxEngine.loadPatch(patches[selectedPatch]);
    return true;
}

bool WebxedSession::selectPreviewEngine(int engineIndex) {
    if (engineIndex == 0) {
        previewEngine = PreviewEngine::dx;
        dxEngine.noteOff();
        return true;
    }
    if (engineIndex == 1) {
        previewEngine = PreviewEngine::digitone;
        digitoneEngine.noteOff();
        return true;
    }
    return false;
}

void WebxedSession::noteOn(int midiNote, double velocity) {
    if (previewEngine == PreviewEngine::dx) {
        dxEngine.noteOn(midiNote, velocity);
    } else {
        digitoneEngine.noteOn(midiNote, velocity);
    }
}

void WebxedSession::noteOff() {
    if (previewEngine == PreviewEngine::dx) {
        dxEngine.noteOff();
    } else {
        digitoneEngine.noteOff();
    }
}

double WebxedSession::renderSample() {
    return previewEngine == PreviewEngine::dx
        ? dxEngine.renderSample()
        : digitoneEngine.renderSample();
}
