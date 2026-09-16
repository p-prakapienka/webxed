#pragma once

#include "model/dx/ConversionReport.h"
#include "model/dx/DxPatch.h"

#include <string>

class ConversionJson {
public:
    static std::string snapshot(
        bool converted,
        const DxPatch& source,
        const ConversionResult& conversion
    );
};
