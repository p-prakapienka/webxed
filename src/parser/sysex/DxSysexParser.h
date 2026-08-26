#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "model/DxPatch.h"

class DxSysexParser {
public:
    std::vector<DxPatch> parse(std::span<const uint8_t> bytes) const;

private:
    static bool validChecksum(std::span<const uint8_t> data, uint8_t checksum);
    static DxPatch parseSingleVoice(std::span<const uint8_t> data);
    static DxPatch unpackBulkVoice(std::span<const uint8_t, 128> packed);
};
