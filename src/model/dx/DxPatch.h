#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

class DxPatch {
public:
    static constexpr std::size_t size = 156;

    explicit DxPatch(std::array<uint8_t, size> data);

    const std::array<uint8_t, size>& getData() const;
    std::string getName() const;
    int getAlgorithm() const;
    static DxPatch initVoice();

private:
    std::array<uint8_t, size> values;
};
