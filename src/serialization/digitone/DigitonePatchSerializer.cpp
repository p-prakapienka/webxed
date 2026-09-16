#include "serialization/digitone/DigitonePatchSerializer.h"

#include <nlohmann/json.hpp>

#include <stdexcept>
#include <string>

namespace {

using Json = nlohmann::json;

constexpr const char* formatName = "webxed-digitone-patch";

Json serializeEnvelope(const DigitoneEnvelope& envelope) {
    return {
        {"attack", envelope.getAttack()},
        {"decay", envelope.getDecay()},
        {"endLevel", envelope.getEndLevel()},
        {"level", envelope.getLevel()},
        {"delay", envelope.getDelay()},
        {"triggered", envelope.getTriggered()},
        {"reset", envelope.getReset()}
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
        {"name", patch.getName()},
        {"algorithm", patch.getAlgorithm()},
        {"ratios", {
            {"c", patch.getRatioC()},
            {"a", patch.getRatioA()},
            {"b1", patch.getRatioB1()},
            {"b2", patch.getRatioB2()}
        }},
        {"harmonic", patch.getHarmonic()},
        {"detune", patch.getDetune()},
        {"feedback", patch.getFeedback()},
        {"mix", patch.getMix()},
        {"envelopes", {
            {"a", serializeEnvelope(patch.getEnvelopeA())},
            {"b", serializeEnvelope(patch.getEnvelopeB())}
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
