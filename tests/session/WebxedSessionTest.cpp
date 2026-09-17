#include "wasm/WebxedSession.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void expect(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::vector<uint8_t> singleVoiceSysex(const DxPatch& patch) {
    std::vector<uint8_t> bytes(6 + 155 + 2, 0);
    bytes[0] = 0xF0;
    bytes[1] = 0x43;
    bytes[3] = 0x00;
    bytes[4] = 1;
    bytes[5] = 27;
    const auto& data = patch.getData();
    std::copy(data.begin(), data.begin() + 155, bytes.begin() + 6);

    uint32_t sum = 0;
    for (std::size_t index = 0; index < 155; ++index) {
        sum += bytes[6 + index];
    }
    bytes[6 + 155] = static_cast<uint8_t>((128 - (sum & 0x7F)) & 0x7F);
    bytes.back() = 0xF7;
    return bytes;
}

void convertWiresMapperAndLeavesSourceUnchanged() {
    WebxedSession session(44100.0);
    const std::string sourceName = session.getPatchName(0);

    expect(!session.selectPreviewEngine(1), "digitone preview requires convert");
    expect(session.convert(), "convert succeeds on init voice");
    expect(session.selectPreviewEngine(1), "digitone preview after convert");
    expect(session.getPatchName(0) == sourceName, "convert must not change the source name");

    const auto json = nlohmann::json::parse(session.getConversionJson());
    expect(json.at("converted").get<bool>(), "conversionJson reports converted");
    expect(json.at("source").at("name").get<std::string>() == sourceName, "source name preserved");
    expect(json.at("source").at("algorithm").get<int>() == 32, "init voice is DX algorithm 32");
    expect(json.at("target").at("name").get<std::string>().find("DN") != std::string::npos,
        "converted name is tagged");
    expect(json.at("target").at("algorithm").get<int>() >= 1, "digitone algorithm lower bound");
    expect(json.at("target").at("algorithm").get<int>() <= 8, "digitone algorithm upper bound");
    expect(json.at("patch").at("format").get<std::string>() == "webxed-digitone-patch",
        "serialized patch uses webxed format");
}

void convertedAudioIsFinite() {
    WebxedSession session(44100.0);
    expect(session.convert(), "convert");
    expect(session.selectPreviewEngine(1), "select digitone");
    session.noteOn(69, 0.8);
    for (int sample = 0; sample < 2048; ++sample) {
        const double value = session.renderSample();
        expect(std::isfinite(value), "digitone renderSample finite");
        expect(std::abs(value) < 8.0, "digitone renderSample bounded");
    }
    session.noteOff();
}

void loadSysexClearsConversion() {
    WebxedSession session(44100.0);
    expect(session.convert(), "convert");
    expect(session.selectPatch(0), "reselecting the same patch keeps conversion");
    expect(nlohmann::json::parse(session.getConversionJson()).at("converted").get<bool>(),
        "same patch still converted");

    const auto sysex = singleVoiceSysex(DxPatch::initVoice());
    expect(session.loadSysex(sysex.data(), sysex.size()) == 1, "reload single-voice sysex");
    expect(!nlohmann::json::parse(session.getConversionJson()).at("converted").get<bool>(),
        "loadSysex clears conversion");
    expect(!session.selectPreviewEngine(1), "digitone preview disabled until convert");
}

void convertIsDeterministic() {
    WebxedSession first(44100.0);
    WebxedSession second(44100.0);
    expect(first.convert(), "first convert");
    expect(second.convert(), "second convert");
    const auto left = nlohmann::json::parse(first.getConversionJson());
    const auto right = nlohmann::json::parse(second.getConversionJson());
    expect(left.at("patch") == right.at("patch"), "session convert is deterministic");
    expect(left.at("report").at("selectedAlgorithm") == right.at("report").at("selectedAlgorithm"),
        "report algorithm deterministic");
}

void applyDigitoneJsonUpdatesConvertedPatchWithoutTouchingSource() {
    WebxedSession session(44100.0);
    expect(!session.applyDigitoneJson("{}"), "apply requires a converted patch");
    expect(session.convert(), "convert");
    const auto before = nlohmann::json::parse(session.getConversionJson());
    const std::string sourceName = before.at("source").at("name").get<std::string>();
    const int sourceAlgorithm = before.at("source").at("algorithm").get<int>();

    auto patch = before.at("patch");
    patch["algorithm"] = patch.at("algorithm").get<int>() == 8 ? 1 : 8;
    patch["ratios"]["c"] = 4.0;
    patch["mix"] = 12;
    patch["name"] = "EDITED DN";
    expect(session.applyDigitoneJson(patch.dump().c_str()), "valid edited patch applies");

    const auto after = nlohmann::json::parse(session.getConversionJson());
    expect(after.at("converted").get<bool>(), "edit keeps conversion");
    expect(after.at("source").at("name").get<std::string>() == sourceName, "source name unchanged");
    expect(after.at("source").at("algorithm").get<int>() == sourceAlgorithm, "source algorithm unchanged");
    expect(after.at("target").at("name").get<std::string>() == "EDITED DN", "target name follows edit");
    expect(after.at("target").at("algorithm").get<int>() == patch.at("algorithm").get<int>(),
        "target algorithm follows edit");
    expect(after.at("patch").at("ratios").at("c").get<double>() == 4.0, "ratio C follows edit");
    expect(after.at("patch").at("mix").get<int>() == 12, "mix follows edit");
    expect(session.selectPreviewEngine(1), "digitone preview still available after edit");

    expect(!session.applyDigitoneJson("{\"format\":\"nope\"}"), "invalid json is rejected");
    const auto rejected = nlohmann::json::parse(session.getConversionJson());
    expect(rejected.at("patch").at("name").get<std::string>() == "EDITED DN",
        "rejected apply must not mutate the converted patch");
}

} // namespace

int main() {
    try {
        convertWiresMapperAndLeavesSourceUnchanged();
        convertedAudioIsFinite();
        loadSysexClearsConversion();
        convertIsDeterministic();
        applyDigitoneJsonUpdatesConvertedPatchWithoutTouchingSource();
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
