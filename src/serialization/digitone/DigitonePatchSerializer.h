#pragma once

#include "model/digitone/DigitonePatch.h"

#include <string>
#include <string_view>

class DigitonePatchSerializer {
public:
    static constexpr int currentVersion = 1;

    std::string serialize(const DigitonePatch& patch) const;
    DigitonePatch deserialize(std::string_view json) const;
};
