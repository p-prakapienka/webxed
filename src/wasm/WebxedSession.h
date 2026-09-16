#pragma once

#include "mapper/DxDigitoneMapper.h"
#include "model/dx/ConversionReport.h"
#include "parser/sysex/DxSysexParser.h"
#include "synth/digitone/DigitoneEngine.h"
#include "synth/dx/DxEngine.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class WebxedSession {
public:
    explicit WebxedSession(double sampleRate);

    int loadSysex(const uint8_t* data, std::size_t size);
    int getPatchCount() const;
    const char* getPatchName(int index);
    bool selectPatch(int index);
    bool selectPreviewEngine(int engineIndex);
    bool convert();
    const char* getConversionJson();

    void noteOn(int midiNote, double velocity);
    void noteOff();
    double renderSample();

private:
    enum class PreviewEngine { dx, digitone };

    void clearConversion();

    DxEngine dxEngine;
    DigitoneEngine digitoneEngine;
    DxSysexParser parser;
    DxDigitoneMapper mapper;
    std::vector<DxPatch> patches;
    std::size_t selectedPatch = 0;
    std::string nameBuffer;
    std::string jsonBuffer;
    ConversionResult conversion;
    bool converted = false;
    PreviewEngine previewEngine = PreviewEngine::dx;
};
