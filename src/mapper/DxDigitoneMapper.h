#pragma once

#include "model/dx/ConversionReport.h"
#include "model/dx/DxPatch.h"

struct ConversionContext;

class DxDigitoneMapper {
public:
    ConversionResult convert(const DxPatch& source);

private:
    void normaliseOutput(ConversionContext& context);
};
