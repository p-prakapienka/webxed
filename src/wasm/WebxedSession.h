#pragma once

#include "parser/sysex/DxSysexParser.h"
#include "synth/dx/DxEngine.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class WebxedSession {
public:
    explicit WebxedSession(double sampleRate);

    int loadSysex(const uint8_t* data, std::size_t size);
    int patchCount() const;
    const char* patchName(int index);
    bool selectPatch(int index);

    void noteOn(int midiNote, double velocity);
    void noteOff();
    double renderSample();

private:
    DxEngine engine;
    DxSysexParser parser;
    std::vector<DxPatch> patches;
    std::size_t selectedPatch = 0;
    std::string nameBuffer;
};
