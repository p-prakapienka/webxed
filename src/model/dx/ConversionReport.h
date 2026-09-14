#pragma once

#include "model/digitone/DigitonePatch.h"

#include <string>
#include <vector>

struct ConversionReport {
    static constexpr int converterVersion = 1;

    std::vector<int> removedOperators;
    std::vector<std::string> mergedOperators;
    int selectedAlgorithm = 1;
    std::vector<std::string> warnings;
    double ratioApproximationError = 0.0;
};

struct ConversionResult {
    DigitonePatch patch;
    ConversionReport report;
};
