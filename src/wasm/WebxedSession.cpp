#include "wasm/WebxedSession.h"

#include "serialization/ConversionSnapshotSerializer.h"

#include <span>

WebxedSession::WebxedSession(double sampleRate)
    : dxEngine(sampleRate), digitoneEngine(sampleRate), patches{DxPatch::initVoice()} {
    dxEngine.loadPatch(patches.front());
}

int WebxedSession::loadSysex(const uint8_t* data, std::size_t size) {
    try {
        patches = parser.parse(std::span<const uint8_t>(data, size));
        selectedPatch = 0;
        clearConversion();
        dxEngine.loadPatch(patches.front());
        return static_cast<int>(patches.size());
    } catch (...) {
        return -1;
    }
}

int WebxedSession::getPatchCount() const {
    return static_cast<int>(patches.size());
}

const char* WebxedSession::getPatchName(int index) {
    if (index < 0 || static_cast<std::size_t>(index) >= patches.size()) {
        nameBuffer.clear();
        return nameBuffer.c_str();
    }
    nameBuffer = patches[static_cast<std::size_t>(index)].getName();
    return nameBuffer.c_str();
}

bool WebxedSession::selectPatch(int index) {
    if (index < 0 || static_cast<std::size_t>(index) >= patches.size()) {
        return false;
    }
    if (selectedPatch == static_cast<std::size_t>(index)) {
        return true;
    }
    selectedPatch = static_cast<std::size_t>(index);
    clearConversion();
    dxEngine.loadPatch(patches[selectedPatch]);
    return true;
}

bool WebxedSession::selectPreviewEngine(int engineIndex) {
    if (engineIndex == 0) {
        previewEngine = PreviewEngine::dx;
        dxEngine.noteOff();
        return true;
    }
    if (engineIndex == 1 && converted) {
        previewEngine = PreviewEngine::digitone;
        digitoneEngine.noteOff();
        return true;
    }
    return false;
}

bool WebxedSession::convert() {
    if (patches.empty()) {
        return false;
    }

    const DxPatch& source = patches[selectedPatch];
    conversion = mapper.convert(source);
    converted = true;
    digitoneEngine.loadPatch(conversion.patch);
    digitoneEngine.noteOff();
    return true;
}

const char* WebxedSession::getConversionJson() {
    jsonBuffer = ConversionSnapshotSerializer().serialize(converted, patches[selectedPatch], conversion);
    return jsonBuffer.c_str();
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

void WebxedSession::clearConversion() {
    converted = false;
    conversion = {};
    previewEngine = PreviewEngine::dx;
    digitoneEngine.noteOff();
}
