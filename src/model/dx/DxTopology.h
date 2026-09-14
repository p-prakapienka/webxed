#pragma once

#include <string>

// Value model of one DX FM topology (one of 32 DX algorithms).
// Index 0 corresponds to DxPatch operator 0 (Yamaha OP6);
// index 5 maps to OP1.
class DxTopology {
public:
    static constexpr int algorithmCount = 32;
    static constexpr int operatorCount = 6;

    explicit DxTopology(int algorithm);

    int getAlgorithm() const;
    bool isCarrier(int op) const;
    bool isFeedbackOperator(int op) const;
    int getInputBus(int op) const;
    int getOutputBus(int op) const;

    // Yamaha OP number for user-facing messages (index 0 is OP6).
    static int getOpNumber(int index);
    static std::string getOpLabel(int index);

private:
    int algorithm_;
    static int flagsFor(int algorithm, int op);
};
