#include "serialization/digitone/DigitonePatchSerializer.h"

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>

namespace {

using Json = nlohmann::json;

constexpr const char* formatName = "webxed-digitone-patch";

Json serializeEnvelope(const DigitoneEnvelope& envelope) {
    return {
        {"attack", envelope.attack()},
        {"decay", envelope.decay()},
        {"endLevel", envelope.endLevel()},
        {"level", envelope.level()},
        {"delay", envelope.delay()},
        {"triggered", envelope.triggered()},
        {"reset", envelope.reset()}
    };
}

DigitoneEnvelope deserializeEnvelope(const Json& json) {
    return DigitoneEnvelope(
        json.at("attack").get<int>(),
        json.at("decay").get<int>(),
        json.at("endLevel").get<int>(),
        json.at("level").get<int>(),
        json.at("delay").get<int>(),
        json.at("triggered").get<bool>(),
        json.at("reset").get<bool>()
    );
}

} // namespace

std::string DigitonePatchSerializer::serialize(const DigitonePatch& patch) const {
    const Json json = {
        {"format", formatName},
        {"version", currentVersion},
        {"name", patch.name()},
        {"algorithm", patch.algorithm()},
        {"ratios", {
            {"c", patch.ratioC()},
            {"a", patch.ratioA()},
            {"b1", patch.ratioB1()},
            {"b2", patch.ratioB2()}
        }},
        {"harmonic", patch.harmonic()},
        {"detune", patch.detune()},
        {"feedback", patch.feedback()},
        {"mix", patch.mix()},
        {"envelopes", {
            {"a", serializeEnvelope(patch.envelopeA())},
            {"b", serializeEnvelope(patch.envelopeB())}
        }}
    };

    return json.dump(2);
}

DigitonePatch DigitonePatchSerializer::deserialize(std::string_view value) const {
    try {
        const Json json = Json::parse(value);
        if (json.at("format").get<std::string>() != formatName) {
            throw std::invalid_argument("unsupported Digitone patch format");
        }
        if (json.at("version").get<int>() != currentVersion) {
            throw std::invalid_argument("unsupported Digitone patch version");
        }

        const Json& ratios = json.at("ratios");
        const Json& envelopes = json.at("envelopes");

        DigitonePatch patch;
        patch.setName(json.at("name").get<std::string>());
        patch.setAlgorithm(json.at("algorithm").get<int>());
        patch.setRatioC(ratios.at("c").get<double>());
        patch.setRatioA(ratios.at("a").get<double>());
        patch.setRatioB1(ratios.at("b1").get<double>());
        patch.setRatioB2(ratios.at("b2").get<double>());
        patch.setHarmonic(json.at("harmonic").get<double>());
        patch.setDetune(json.at("detune").get<int>());
        patch.setFeedback(json.at("feedback").get<int>());
        patch.setMix(json.at("mix").get<int>());
        patch.setEnvelopeA(deserializeEnvelope(envelopes.at("a")));
        patch.setEnvelopeB(deserializeEnvelope(envelopes.at("b")));
        return patch;
    } catch (const std::invalid_argument&) {
        throw;
    } catch (const std::exception& exception) {
        throw std::invalid_argument(
            std::string("invalid Digitone patch JSON: ") + exception.what()
        );
    }
}
