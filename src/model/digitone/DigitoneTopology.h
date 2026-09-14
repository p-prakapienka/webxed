#pragma once

#include <array>
#include <vector>

// Value model of one Digitone FM topology (one of 8 algorithms).
// Slot order: 0=C, 1=A, 2=B1, 3=B2. Mirrors DigitoneEngine parents.
class DigitoneTopology {
public:
    static constexpr int algorithmCount = 8;
    static constexpr int slotCount = 4;

    explicit DigitoneTopology(int algorithm);

    int getAlgorithm() const;
    const std::array<int, slotCount>& getParents() const;
    std::vector<int> getCarrierSlots() const;
    std::vector<int> getCarrierSlotsByWeight() const;
    std::vector<int> getModulatorSlots() const;
    // Modulator slots closest to a carrier come first per topology.
    std::vector<int> getModulatorPriority() const;

    static double snapToNearestRatio(double ideal, double& error);

private:
    int algorithm_;
};
