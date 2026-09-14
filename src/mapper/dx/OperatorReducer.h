#pragma once

class DxPatch;
struct ConversionContext;
struct OperatorView;

// Reduces six DX operators to four: parses the source patch into scored
// operator views, then keeps the four highest-value operators while
// preserving carriers and the feedback operator where possible.
class OperatorReducer {
public:
    void reduce(ConversionContext& context, const DxPatch& source);

private:
    void analyze(ConversionContext& context, const DxPatch& source);
    void select(ConversionContext& context);
    double idealRatio(const OperatorView& op);
};
