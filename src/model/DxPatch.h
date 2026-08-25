#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

class DxPatch {
public:
    static constexpr std::size_t size = 156;

    explicit DxPatch(std::array<uint8_t, size> data);

    const std::array<uint8_t, size>& data() const;
    static DxPatch initVoice();

private:
    std::array<uint8_t, size> values;
};
