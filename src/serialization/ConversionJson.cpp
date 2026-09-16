#include "serialization/ConversionJson.h"

#include "serialization/digitone/DigitonePatchSerializer.h"

#include <nlohmann/json.hpp>

namespace {

using Json = nlohmann::json;

Json reportJson(const ConversionReport& report) {
    return {
        {"removedOperators", report.removedOperators},
        {"mergedOperators", report.mergedOperators},
        {"selectedAlgorithm", report.selectedAlgorithm},
        {"warnings", report.warnings},
        {"ratioApproximationError", report.ratioApproximationError},
        {"converterVersion", ConversionReport::converterVersion}
    };
}

} // namespace

std::string ConversionJson::snapshot(
    bool converted,
    const DxPatch& source,
    const ConversionResult& conversion
) {
    Json json = {
        {"converted", converted},
        {"source", {
            {"name", source.getName()},
            {"algorithm", source.getAlgorithm()}
        }}
    };

    if (converted) {
        json["target"] = {
            {"name", conversion.patch.getName()},
            {"algorithm", conversion.patch.getAlgorithm()}
        };
        json["report"] = reportJson(conversion.report);
        json["patch"] = DigitonePatchSerializer().toJson(conversion.patch);
    }

    return json.dump();
}
