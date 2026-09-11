#include "mapper/DxDigitoneMapper.h"
#include "model/dx/DxPatch.h"
#include "synth/digitone/DigitoneEngine.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void expect(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

DxPatch patchWith(std::array<uint8_t, DxPatch::size> bytes) {
    return DxPatch(bytes);
}

std::array<uint8_t, DxPatch::size> initBytes() {
    return DxPatch::initVoice().data();
}

void setOperator(std::array<uint8_t, DxPatch::size>& bytes, int op, int outputLevel, int mode, int coarse, int fine) {
    const std::size_t offset = static_cast<std::size_t>(op) * 21;
    bytes[offset + 16] = static_cast<uint8_t>(outputLevel);
    bytes[offset + 17] = static_cast<uint8_t>(mode);
    bytes[offset + 18] = static_cast<uint8_t>(coarse);
    bytes[offset + 19] = static_cast<uint8_t>(fine);
}

void expectValidPatch(const DigitonePatch& patch) {
    expect(patch.algorithm() >= 1 && patch.algorithm() <= 8, "converted algorithm in range");
    for (double ratio : {patch.ratioC(), patch.ratioA(), patch.ratioB1(), patch.ratioB2()}) {
        expect(std::isfinite(ratio), "ratio finite");
        expect(ratio >= 0.25 && ratio <= 16.0, "ratio in range");
    }
    expect(patch.detune() >= 0 && patch.detune() <= 127, "detune in range");
    expect(patch.feedback() >= 0 && patch.feedback() <= 127, "feedback in range");
    expect(patch.mix() >= -64 && patch.mix() <= 63, "mix in range");
}

void conversionIsDeterministic() {
    DxDigitoneMapper converter;
    const DxPatch source = DxPatch::initVoice();
    const ConversionResult first = converter.convert(source);
    const ConversionResult second = converter.convert(source);
    expect(first.patch == second.patch, "converter must be deterministic");
    expect(first.report.selectedAlgorithm == second.report.selectedAlgorithm, "report deterministic");
    expect(first.report.removedOperators == second.report.removedOperators, "removed operators deterministic");
}

void sourceIsNotMutated() {
    const DxPatch source = DxPatch::initVoice();
    const auto before = source.data();
    DxDigitoneMapper converter;
    converter.convert(source);
    expect(source.data() == before, "conversion must not mutate the source patch");
}

void initVoiceKeepsLoudestOperator() {
    DxDigitoneMapper converter;
    const ConversionResult result = converter.convert(DxPatch::initVoice());
    expect(result.report.removedOperators.size() == 2, "exactly two operators removed");
    for (int removed : result.report.removedOperators) {
        expect(removed != 0, "loudest operator must survive reduction");
    }
    expect(result.report.mergedOperators.empty(), "first pass merges nothing");
    expectValidPatch(result.patch);
}

void allDxAlgorithmsConvert() {
    DxDigitoneMapper converter;
    for (int algorithm = 0; algorithm < 32; ++algorithm) {
        auto bytes = initBytes();
        bytes[134] = static_cast<uint8_t>(algorithm);
        for (int op = 0; op < 6; ++op) {
            setOperator(bytes, op, 80, 0, 1, 0);
        }
        const ConversionResult result = converter.convert(patchWith(bytes));
        expectValidPatch(result.patch);
        expect(result.report.removedOperators.size() == 2, "two operators removed for every algorithm");
        expect(result.report.selectedAlgorithm >= 1 && result.report.selectedAlgorithm <= 8, "target algorithm valid");
    }
}

void feedbackMapsProportionally() {
    auto silent = initBytes();
    silent[135] = 0;
    DxDigitoneMapper converter;
    expect(converter.convert(patchWith(silent)).patch.feedback() == 0, "zero feedback maps to zero");

    auto full = initBytes();
    full[135] = 7;
    const ConversionResult result = converter.convert(patchWith(full));
    expect(result.patch.feedback() == 127, "maximum DX feedback maps to maximum Digitone feedback");
}

void exactRatiosHaveNoError() {
    auto bytes = initBytes();
    bytes[134] = 31;
    for (int op = 0; op < 6; ++op) {
        setOperator(bytes, op, 80, 0, 1, 0);
    }
    DxDigitoneMapper converter;
    const ConversionResult result = converter.convert(patchWith(bytes));
    expect(result.patch.ratioC() == 1.0, "exact ratio preserved");
    expect(result.patch.ratioA() == 1.0, "exact ratio preserved");
    expect(result.patch.ratioB1() == 1.0, "exact ratio preserved");
    expect(result.patch.ratioB2() == 1.0, "exact ratio preserved");
    expect(result.report.ratioApproximationError == 0.0, "no approximation error for exact ratios");
}

void fixedFrequencyWarns() {
    auto bytes = initBytes();
    bytes[134] = 31;
    setOperator(bytes, 0, 99, 1, 0, 0);
    DxDigitoneMapper converter;
    const ConversionResult result = converter.convert(patchWith(bytes));
    expectValidPatch(result.patch);
    bool warned = false;
    for (const std::string& warning : result.report.warnings) {
        if (warning.find("fixed") != std::string::npos) {
            warned = true;
        }
    }
    expect(warned, "fixed-frequency operators must warn");
}

void envelopeAttackFollowsDxRate() {
    auto fastBytes = initBytes();
    fastBytes[134] = 31;
    fastBytes[0] = 99;
    auto slowBytes = fastBytes;
    slowBytes[0] = 0;

    DxDigitoneMapper converter;
    const ConversionResult fast = converter.convert(patchWith(fastBytes));
    const ConversionResult slow = converter.convert(patchWith(slowBytes));
    expect(fast.patch.envelopeA().attack() < slow.patch.envelopeA().attack(), "faster DX rate must give shorter attack");
}

void envelopeTriggerFollowsSustainLevel() {
    auto percussive = initBytes();
    percussive[134] = 31;
    percussive[6] = 0;
    auto sustained = percussive;
    sustained[6] = 99;

    DxDigitoneMapper converter;
    expect(converter.convert(patchWith(percussive)).patch.envelopeA().triggered(), "zero sustain must trigger decay");
    expect(!converter.convert(patchWith(sustained)).patch.envelopeA().triggered(), "sustain level must hold");
}

void convertedPatchRendersFiniteAudio() {
    DxDigitoneMapper converter;
    const ConversionResult result = converter.convert(DxPatch::initVoice());
    DigitoneEngine engine(48000.0);
    engine.loadPatch(result.patch);
    engine.noteOn(69, 0.8);
    double peak = 0.0;
    for (int i = 0; i < 4096; ++i) {
        const double sample = engine.renderSample();
        expect(std::isfinite(sample), "converted patch must render finite audio");
        expect(std::abs(sample) <= 1.0, "converted patch must render bounded audio");
        peak = std::max(peak, std::abs(sample));
    }
    expect(peak > 0.001, "converted init voice must be audible");
}

} // namespace

int main() {
    try {
        conversionIsDeterministic();
        sourceIsNotMutated();
        initVoiceKeepsLoudestOperator();
        allDxAlgorithmsConvert();
        feedbackMapsProportionally();
        exactRatiosHaveNoError();
        fixedFrequencyWarns();
        envelopeAttackFollowsDxRate();
        envelopeTriggerFollowsSustainLevel();
        convertedPatchRendersFiniteAudio();
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
