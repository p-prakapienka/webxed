#pragma once

#include "model/dx/ConversionReport.h"
#include "model/dx/DxPatch.h"
#include "model/digitone/DigitonePatch.h"

#include <array>
#include <cstdint>

struct OperatorView {
    int index = 0;
    bool carrier = false;
    bool feedback = false;
    int outputLevel = 0;
    int mode = 0;
    int coarse = 0;
    int fine = 0;
    int detune = 0;
    std::array<int, 4> rates{};
    std::array<int, 4> levels{};
    int velocitySensitivity = 0;
    double idealRatio = 1.0;
    double score = 0.0;
};

struct ConversionContext {
    const DxPatch* sourcePatch = nullptr;
    std::array<uint8_t, DxPatch::size> sourceBytes{};
    std::array<OperatorView, 6> operators{};
    int dxAlgorithm = 0;
    int dxFeedback = 0;
    int feedbackOp = -1;
    std::array<int, 4> slotToDx{{-1, -1, -1, -1}};
    std::array<bool, 6> selected{};
    int dnAlgorithm = 1;
    DigitonePatch target;
    ConversionReport report;
};
