#include "model/digitone/DigitoneTopology.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace {

constexpr std::array<std::array<int, 4>, 8> dnParents = {{
    {{1, 2, 3, -1}},
    {{1, 2, 3, -1}},
    {{1, 2, -1, -1}},
    {{-1, 2, -1, -1}},
    {{1, -1, 2, -1}},
    {{-1, 2, 2, -1}},
    {{1, -1, 1, -1}},
    {{-1, 0, 0, 0}},
}};

constexpr std::array<double, 21> targetRatios = {{
    0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0, 2.25, 2.5, 3.0,
    3.5, 4.0, 5.0, 6.0, 7.0, 8.0, 10.0, 12.0, 14.0, 16.0,
}};

// Carrier slots ordered by engine output weight (C=1.0, A/B1=0.5, B2=0.25).
constexpr std::array<double, 4> slotWeight{{1.0, 0.5, 0.5, 0.25}};

bool isCarrierSlot(const std::array<int, 4>& parents, int slot) {
    const int parent = parents[static_cast<std::size_t>(slot)];
    return parent < 0 || parent == slot;
}

} // namespace

DigitoneTopology::DigitoneTopology(int algorithm)
    : algorithm_(std::clamp(algorithm, 1, algorithmCount)) {}

int DigitoneTopology::getAlgorithm() const {
    return algorithm_;
}

const std::array<int, DigitoneTopology::slotCount>& DigitoneTopology::getParents() const {
    return dnParents[static_cast<std::size_t>(algorithm_ - 1)];
}

std::vector<int> DigitoneTopology::getCarrierSlots() const {
    const auto& parentList = getParents();
    std::vector<int> slots;
    for (int slot = 0; slot < slotCount; ++slot) {
        if (isCarrierSlot(parentList, slot)) {
            slots.push_back(slot);
        }
    }
    return slots;
}

std::vector<int> DigitoneTopology::getCarrierSlotsByWeight() const {
    std::vector<int> slots = getCarrierSlots();
    std::sort(slots.begin(), slots.end(), [](int a, int b) {
        if (slotWeight[static_cast<std::size_t>(a)] != slotWeight[static_cast<std::size_t>(b)]) {
            return slotWeight[static_cast<std::size_t>(a)] > slotWeight[static_cast<std::size_t>(b)];
        }
        return a < b;
    });
    return slots;
}

std::vector<int> DigitoneTopology::getModulatorSlots() const {
    const auto& parentList = getParents();
    std::vector<int> slots;
    for (int slot = 0; slot < slotCount; ++slot) {
        if (!isCarrierSlot(parentList, slot)) {
            slots.push_back(slot);
        }
    }
    return slots;
}

std::vector<int> DigitoneTopology::getModulatorPriority() const {
    switch (algorithm_) {
    case 1:
    case 2:
        return {2, 1, 0};
    case 3:
        return {1, 0};
    case 7:
        return {0, 2};
    case 8:
        return {1, 2, 3};
    default:
        return getModulatorSlots();
    }
}

double DigitoneTopology::snapToNearestRatio(double ideal, double& error) {
    double best = targetRatios.front();
    double bestError = std::abs(ideal - best);
    for (double candidate : targetRatios) {
        const double candidateError = std::abs(ideal - candidate);
        if (candidateError < bestError) {
            bestError = candidateError;
            best = candidate;
        }
    }
    error = bestError;
    return best;
}
