#pragma once

#include "model/digitone/DigitonePatch.h"

#include <nlohmann/json.hpp>
#include <string>
#include <string_view>

class DigitonePatchSerializer {
public:
    static constexpr int currentVersion = 1;

    nlohmann::json toJson(const DigitonePatch& patch) const;
    std::string serialize(const DigitonePatch& patch) const;
    DigitonePatch deserialize(std::string_view json) const;
};
