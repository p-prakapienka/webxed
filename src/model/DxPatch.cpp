#include "model/DxPatch.h"

#include <algorithm>
#include <string_view>

DxPatch::DxPatch(std::array<uint8_t, size> data) : values(data) {}

const std::array<uint8_t, DxPatch::size>& DxPatch::data() const {
    return values;
}

std::string DxPatch::name() const {
    std::string result(values.begin() + 145, values.begin() + 155);
    while (!result.empty() && result.back() == ' ') {
        result.pop_back();
    }
    return result;
}

DxPatch DxPatch::initVoice() {
    std::array<uint8_t, size> patch{};

    for (std::size_t op = 0; op < 6; ++op) {
        const std::size_t offset = op * 21;

        patch[offset + 0] = 99;
        patch[offset + 1] = 99;
        patch[offset + 2] = 99;
        patch[offset + 3] = 99;

        patch[offset + 4] = 99;
        patch[offset + 5] = 99;
        patch[offset + 6] = 99;
        patch[offset + 7] = 0;

        patch[offset + 8] = 39;
        patch[offset + 9] = 0;
        patch[offset + 10] = 0;
        patch[offset + 11] = 0;
        patch[offset + 12] = 0;
        patch[offset + 13] = 0;
        patch[offset + 14] = 0;
        patch[offset + 15] = 0;
        patch[offset + 16] = op == 0 ? 99 : 0;
        patch[offset + 17] = 0;
        patch[offset + 18] = 1;
        patch[offset + 19] = 0;
        patch[offset + 20] = 7;
    }

    for (std::size_t index = 126; index < 130; ++index) {
        patch[index] = 99;
    }
    for (std::size_t index = 130; index < 134; ++index) {
        patch[index] = 50;
    }

    patch[134] = 31;
    patch[135] = 0;
    patch[136] = 1;
    patch[144] = 24;

    constexpr std::string_view name = "WEBXEDINIT";
    std::copy(name.begin(), name.end(), patch.begin() + 145);

    return DxPatch(patch);
}
