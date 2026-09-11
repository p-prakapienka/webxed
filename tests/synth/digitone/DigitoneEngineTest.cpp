#include "model/digitone/DigitonePatch.h"
#include "synth/digitone/DigitoneEngine.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
constexpr double sampleRate = 48000.0;

void expect(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

DigitonePatch patchFor(int algorithm) {
    DigitonePatch patch;
    patch.setAlgorithm(algorithm);
    patch.setRatioC(1.0);
    patch.setRatioA(2.0);
    patch.setRatioB1(3.0);
    patch.setRatioB2(6.0);
    patch.setHarmonic(8.0);
    patch.setDetune(16);
    patch.setFeedback(32);
    patch.setMix(0);
    patch.setEnvelopeA(DigitoneEnvelope(1, 40, 20, 127, 0, false, true));
    patch.setEnvelopeB(DigitoneEnvelope(0, 32, 10, 100, 0, false, true));
    return patch;
}

std::vector<double> render(DigitoneEngine& engine, int count) {
    std::vector<double> samples;
    samples.reserve(static_cast<std::size_t>(count));
    for (int index = 0; index < count; ++index) {
        samples.push_back(engine.renderSample());
    }
    return samples;
}

double peak(const std::vector<double>& samples) {
    double result = 0.0;
    for (double sample : samples) {
        result = std::max(result, std::abs(sample));
    }
    return result;
}

void everyAlgorithmProducesFiniteAudio() {
    for (int algorithm = 1; algorithm <= 8; ++algorithm) {
        DigitoneEngine engine(sampleRate);
        engine.loadPatch(patchFor(algorithm));
        engine.noteOn(69, 0.8);
        const auto samples = render(engine, 4096);
        expect(peak(samples) > 0.001, "algorithm must produce audible output");
        for (double sample : samples) {
            expect(std::isfinite(sample), "algorithm output must remain finite");
            expect(std::abs(sample) <= 1.0, "algorithm output must remain bounded");
        }
    }
}

void noteOffReleasesVoice() {
    DigitoneEngine engine(sampleRate);
    engine.loadPatch(patchFor(4));
    engine.noteOn(69, 1.0);
    render(engine, 2048);
    engine.noteOff();
    render(engine, 48000);
    expect(engine.renderSample() == 0.0, "voice must become silent after release");
}

void controlsChangeRenderedVoice() {
    DigitoneEngine first(sampleRate);
    DigitoneEngine second(sampleRate);
    auto firstPatch = patchFor(2);
    auto secondPatch = firstPatch;
    secondPatch.setRatioC(5.0);
    secondPatch.setFeedback(127);
    secondPatch.setHarmonic(-18.0);
    first.loadPatch(firstPatch);
    second.loadPatch(secondPatch);
    first.noteOn(60, 1.0);
    second.noteOn(60, 1.0);
    const auto firstSamples = render(first, 2048);
    const auto secondSamples = render(second, 2048);
    double difference = 0.0;
    for (std::size_t index = 0; index < firstSamples.size(); ++index) {
        difference += std::abs(firstSamples[index] - secondSamples[index]);
    }
    expect(difference > 1.0, "patch controls must affect rendered audio");
}
}

int main() {
    try {
        everyAlgorithmProducesFiniteAudio();
        noteOffReleasesVoice();
        controlsChangeRenderedVoice();
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
    return 0;
}
