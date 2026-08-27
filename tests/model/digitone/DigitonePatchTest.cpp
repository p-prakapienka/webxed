#include "model/digitone/DigitonePatch.h"
#include "serialization/digitone/DigitonePatchSerializer.h"

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void expect(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void expectInvalid(const std::function<void()>& operation, const char* message) {
    try {
        operation();
    } catch (const std::exception&) {
        return;
    }
    throw std::runtime_error(message);
}

DigitonePatch createPatch() {
    DigitonePatch patch;
    patch.setName("Bell \"A/B\"");
    patch.setAlgorithm(8);
    patch.setRatioC(0.5);
    patch.setRatioA(3.25);
    patch.setRatioB1(8.0);
    patch.setRatioB2(16.0);
    patch.setHarmonic(-12.5);
    patch.setDetune(64);
    patch.setFeedback(127);
    patch.setMix(-64);
    patch.setEnvelopeA(DigitoneEnvelope(1, 2, 3, 4, 5, true, false));
    patch.setEnvelopeB(DigitoneEnvelope(127, 126, 125, 124, 123, false, true));
    return patch;
}

void defaultPatchUsesNeutralHardwareValues() {
    const DigitonePatch patch;
    expect(patch.name() == "INIT", "default name");
    expect(patch.algorithm() == 1, "default algorithm");
    expect(patch.ratioC() == 1.0, "default C ratio");
    expect(patch.ratioA() == 1.0, "default A ratio");
    expect(patch.ratioB1() == 1.0, "default B1 ratio");
    expect(patch.ratioB2() == 1.0, "default B2 ratio");
    expect(patch.harmonic() == 0.0, "default harmonic");
    expect(patch.detune() == 0, "default detune");
    expect(patch.feedback() == 0, "default feedback");
    expect(patch.mix() == 0, "default mix");
}

void patchRoundTripsThroughVersionedJson() {
    const DigitonePatch original = createPatch();
    const DigitonePatchSerializer serializer;
    const std::string json = serializer.serialize(original);
    const DigitonePatch restored = serializer.deserialize(json);

    expect(restored == original, "serialized patch must round-trip");
    expect(json.find("webxed-digitone-patch") != std::string::npos, "format marker");
    expect(json.find("\\\"A/B\\\"") != std::string::npos, "escaped name");
}

void hardwareRangesAreEnforced() {
    DigitonePatch patch;
    expectInvalid([&patch] { patch.setAlgorithm(0); }, "algorithm lower bound");
    expectInvalid([&patch] { patch.setAlgorithm(9); }, "algorithm upper bound");
    expectInvalid([&patch] { patch.setRatioA(0.24); }, "ratio lower bound");
    expectInvalid([&patch] { patch.setRatioB2(16.01); }, "ratio upper bound");
    expectInvalid([&patch] { patch.setHarmonic(26.01); }, "harmonic upper bound");
    expectInvalid([&patch] { patch.setDetune(128); }, "detune upper bound");
    expectInvalid([&patch] { patch.setFeedback(-1); }, "feedback lower bound");
    expectInvalid([&patch] { patch.setMix(-65); }, "mix lower bound");
    expectInvalid([] { DigitoneEnvelope(0, 0, 0, 0, 128, false, false); }, "envelope range");
}

void incompatibleDocumentsAreRejected() {
    const DigitonePatchSerializer serializer;
    expectInvalid(
        [&serializer] { serializer.deserialize(R"({"format":"other","version":1})"); },
        "foreign format"
    );

    std::string json = serializer.serialize(DigitonePatch());
    const std::string version = "\"version\": 1";
    json.replace(json.find(version), version.size(), "\"version\": 2");
    expectInvalid([&serializer, &json] { serializer.deserialize(json); }, "future version");
}

} // namespace

int main() {
    try {
        defaultPatchUsesNeutralHardwareValues();
        patchRoundTripsThroughVersionedJson();
        hardwareRangesAreEnforced();
        incompatibleDocumentsAreRejected();
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
