#include "parser/sysex/DxSysexParser.h"
#include "synth/dx/DxEngine.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

class WebxedSession {
public:
    explicit WebxedSession(double sampleRate) : engine(sampleRate), patches{DxPatch::initVoice()} {
        engine.loadPatch(patches.front());
    }

    int loadSysex(const uint8_t* data, std::size_t size) {
        try {
            patches = parser.parse(std::span<const uint8_t>(data, size));
            selectedPatch = 0;
            engine.loadPatch(patches.front());
            return static_cast<int>(patches.size());
        } catch (...) {
            return -1;
        }
    }

    int patchCount() const {
        return static_cast<int>(patches.size());
    }

    const char* patchName(int index) {
        if (index < 0 || static_cast<std::size_t>(index) >= patches.size()) {
            nameBuffer.clear();
            return nameBuffer.c_str();
        }
        nameBuffer = patches[static_cast<std::size_t>(index)].name();
        return nameBuffer.c_str();
    }

    bool selectPatch(int index) {
        if (index < 0 || static_cast<std::size_t>(index) >= patches.size()) {
            return false;
        }
        selectedPatch = static_cast<std::size_t>(index);
        engine.loadPatch(patches[selectedPatch]);
        return true;
    }

    DxEngine engine;

private:
    DxSysexParser parser;
    std::vector<DxPatch> patches;
    std::size_t selectedPatch = 0;
    std::string nameBuffer;
};

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
    return session != nullptr ? session->patchCount() : 0;
}

const char* patchName(WebxedSession* session, int index) {
    return session != nullptr ? session->patchName(index) : "";
}

int selectPatch(WebxedSession* session, int index) {
    return session != nullptr && session->selectPatch(index) ? 1 : 0;
}

void noteOn(WebxedSession* session, int midiNote, double velocity) {
    if (session != nullptr) {
        session->engine.noteOn(midiNote, velocity);
    }
}

void noteOff(WebxedSession* session) {
    if (session != nullptr) {
        session->engine.noteOff();
    }
}

double renderSample(WebxedSession* session) {
    return session != nullptr ? session->engine.renderSample() : 0.0;
}

}

int main() {
    return 0;
}
