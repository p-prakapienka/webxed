#include "parser/sysex/DxSysexParser.h"

#include <array>
#include <stdexcept>

namespace {
constexpr uint8_t sysexStart = 0xF0;
constexpr uint8_t sysexEnd = 0xF7;
constexpr uint8_t yamahaId = 0x43;
constexpr uint8_t singleVoiceFormat = 0x00;
constexpr uint8_t bulkVoiceFormat = 0x09;
constexpr std::size_t headerSize = 6;
constexpr std::size_t singleVoiceDataSize = 155;
constexpr std::size_t bulkVoiceDataSize = 4096;
constexpr std::size_t packedVoiceSize = 128;
constexpr std::size_t bulkVoiceCount = 32;
}

std::vector<DxPatch> DxSysexParser::parse(std::span<const uint8_t> bytes) const {
    if (bytes.size() < headerSize + 2 || bytes.front() != sysexStart || bytes.back() != sysexEnd) {
        throw std::invalid_argument("Invalid Yamaha SysEx message");
    }
    if (bytes[1] != yamahaId) {
        throw std::invalid_argument("Unsupported SysEx manufacturer");
    }

    const uint8_t format = bytes[3];
    const std::size_t dataSize = (static_cast<std::size_t>(bytes[4]) << 7) | bytes[5];
    if (bytes.size() != headerSize + dataSize + 2) {
        throw std::invalid_argument("Unexpected SysEx byte count");
    }

    const auto data = bytes.subspan(headerSize, dataSize);
    const uint8_t checksum = bytes[headerSize + dataSize];
    if (!validChecksum(data, checksum)) {
        throw std::invalid_argument("Invalid SysEx checksum");
    }

    if (format == singleVoiceFormat && dataSize == singleVoiceDataSize) {
        return {parseSingleVoice(data)};
    }

    if (format == bulkVoiceFormat && dataSize == bulkVoiceDataSize) {
        std::vector<DxPatch> patches;
        patches.reserve(bulkVoiceCount);
        for (std::size_t voice = 0; voice < bulkVoiceCount; ++voice) {
            const auto offset = voice * packedVoiceSize;
            const std::span<const uint8_t, packedVoiceSize> packed(data.data() + offset, packedVoiceSize);
            patches.push_back(unpackBulkVoice(packed));
        }
        return patches;
    }

    throw std::invalid_argument("Unsupported DX7 SysEx format");
}

bool DxSysexParser::validChecksum(std::span<const uint8_t> data, uint8_t checksum) {
    uint32_t sum = 0;
    for (const uint8_t value : data) {
        sum += value;
    }
    return static_cast<uint8_t>((128 - (sum & 0x7F)) & 0x7F) == checksum;
}

DxPatch DxSysexParser::parseSingleVoice(std::span<const uint8_t> data) {
    std::array<uint8_t, DxPatch::size> unpacked{};
    std::copy_n(data.begin(), singleVoiceDataSize, unpacked.begin());
    unpacked[155] = 0x3F;
    return DxPatch(unpacked);
}

DxPatch DxSysexParser::unpackBulkVoice(std::span<const uint8_t, 128> packed) {
    std::array<uint8_t, DxPatch::size> unpacked{};

    for (std::size_t op = 0; op < 6; ++op) {
        const std::size_t packedOffset = op * 17;
        const std::size_t unpackedOffset = op * 21;

        for (std::size_t index = 0; index < 11; ++index) {
            unpacked[unpackedOffset + index] = packed[packedOffset + index];
        }

        const uint8_t curves = packed[packedOffset + 11];
        unpacked[unpackedOffset + 11] = curves & 0x03;
        unpacked[unpackedOffset + 12] = (curves >> 2) & 0x03;

        const uint8_t detuneRate = packed[packedOffset + 12];
        unpacked[unpackedOffset + 13] = detuneRate & 0x07;
        unpacked[unpackedOffset + 20] = (detuneRate >> 3) & 0x0F;

        const uint8_t velocityAmp = packed[packedOffset + 13];
        unpacked[unpackedOffset + 14] = velocityAmp & 0x03;
        unpacked[unpackedOffset + 15] = (velocityAmp >> 2) & 0x07;

        unpacked[unpackedOffset + 16] = packed[packedOffset + 14];

        const uint8_t coarseMode = packed[packedOffset + 15];
        unpacked[unpackedOffset + 17] = coarseMode & 0x01;
        unpacked[unpackedOffset + 18] = (coarseMode >> 1) & 0x1F;
        unpacked[unpackedOffset + 19] = packed[packedOffset + 16];
    }

    for (std::size_t index = 0; index < 8; ++index) {
        unpacked[126 + index] = packed[102 + index];
    }

    unpacked[134] = packed[110] & 0x1F;
    unpacked[135] = packed[111] & 0x07;
    unpacked[136] = (packed[111] >> 3) & 0x01;
    unpacked[137] = packed[112];
    unpacked[138] = packed[113];
    unpacked[139] = packed[114];
    unpacked[140] = packed[115];

    const uint8_t lfo = packed[116];
    unpacked[141] = lfo & 0x01;
    unpacked[142] = (lfo >> 1) & 0x07;
    unpacked[143] = (lfo >> 4) & 0x07;
    unpacked[144] = packed[117];

    for (std::size_t index = 0; index < 10; ++index) {
        unpacked[145 + index] = packed[118 + index];
    }
    unpacked[155] = 0x3F;

    return DxPatch(unpacked);
}
