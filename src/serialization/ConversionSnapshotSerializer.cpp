#include "serialization/ConversionSnapshotSerializer.h"

#include "serialization/digitone/DigitonePatchSerializer.h"

namespace {

nlohmann::json reportJson(const ConversionReport& report) {
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

nlohmann::json ConversionSnapshotSerializer::toJson(
    bool converted,
    const DxPatch& source,
    const ConversionResult& conversion
) const {
    nlohmann::json json = {
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

    return json;
}

std::string ConversionSnapshotSerializer::serialize(
    bool converted,
    const DxPatch& source,
    const ConversionResult& conversion
) const {
    return toJson(converted, source, conversion).dump();
}
