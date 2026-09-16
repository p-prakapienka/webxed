#pragma once

#include "model/dx/ConversionReport.h"
#include "model/dx/DxPatch.h"

#include <nlohmann/json.hpp>
#include <string>

class ConversionSnapshotSerializer {
public:
    nlohmann::json toJson(
        bool converted,
        const DxPatch& source,
        const ConversionResult& conversion
    ) const;
    std::string serialize(
        bool converted,
        const DxPatch& source,
        const ConversionResult& conversion
    ) const;
};
