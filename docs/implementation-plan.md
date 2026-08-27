# Webxed Implementation Plan

Webxed is a browser-based DX7/Dexed preset audition and Digitone II conversion tool.

The primary workflow is:

```text
DX7 SysEx
   ↓
Load and browse patches
   ↓
Preview original DX patch
   ↓
Convert to Digitone-style patch
   ↓
Preview converted patch
   ↓
Tweak
   ↓
Save
```

The project is intentionally focused on fast DX preset browsing, conversion, and A/B audition rather than becoming a complete browser clone of Dexed.

## Architecture principles

- Keep `DxPatch` and `DigitonePatch` as separate domain models.
- Keep `DxEngine` and `DigitoneEngine` as separate synthesis engines.
- Reuse low-level FM components where it makes sense, but avoid a highly configurable universal FM engine.
- Keep DSP independent from the browser UI.
- Keep SysEx packing/unpacking details on the C++ side.
- Keep the WASM C ABI thin; application/session state belongs in objects such as `WebxedSession`.
- Prefer small objects with clear responsibilities over large files or collections of unrelated free functions.
- Conversion must be explicit and lossy. Never mutate the source DX patch during conversion.

## Milestone 1 — Project foundation ✅

Establish the browser/WASM audio path without committing to the final DX implementation.

Delivered:

- C++20 + CMake/Emscripten project foundation.
- Source kept under `src/`.
- Minimal browser shell.
- WASM audio bridge.
- CI build.
- Initial FM test voice used to validate the architecture.

## Milestone 2 — DX/Dexed synthesis core ✅

Integrate the DX synthesis core behind a Webxed-owned engine boundary.

Delivered:

- Dexed/MSFA pinned to a concrete upstream commit.
- Dependency limited to the Apache-2.0 MSFA synthesis core rather than the JUCE/Dexed application.
- `DxPatch` 156-byte unpacked voice model.
- `DxEngine` under `src/synth/dx`.
- Six operators.
- DX algorithm selection.
- Ratio and fixed-frequency modes.
- Feedback.
- MSFA DX operator envelopes.
- Browser audition backed by the DX engine.

## Milestone 3 — SysEx loading and preset browser ✅

Load real DX7 patch files and make browsing fast enough for the intended workflow.

Delivered:

- Single-voice DX7 SysEx parsing.
- 32-voice packed bank parsing.
- Yamaha header, byte-count, and checksum validation.
- Packed bank voice expansion into `DxPatch`.
- Patch names.
- `.syx` browser upload.
- Patch selection and previous/next navigation.
- Keyboard navigation with Left/Right and Space audition.
- `WebxedSession` separated from the thin C ABI bridge.
- Browser responsibilities split into dedicated objects such as `WebxedApi`, `AudioEngine`, `PatchBrowser`, and `SysexLoader`.

## Milestone 4 — Digitone-compatible patch model ✅

Create a target model that reflects Digitone hardware controls rather than convenient internal DSP values.

Conceptual model:

```text
DigitonePatch {
    name
    algorithm
    ratioA
    ratioB1
    ratioB2
    ratioC
    harmonic
    detune
    feedback
    mix
    envelopeA
    envelopeB

    // later
    ampEnvelope
    filter
    velocity
    keyTracking
}
```

Rules:

- Keep hardware-facing parameters in `DigitonePatch`.
- Keep DSP-only implementation details inside `DigitoneEngine`.
- Make the model serializable from the start.

Delivered:

- Encapsulated `DigitonePatch` and `DigitoneEnvelope` domain models.
- Hardware-facing FM Tone parameters and ranges.
- Separate A and B operator envelopes with attack, decay, end level, modulation level, delay, trigger mode, and reset behavior.
- Versioned JSON serialization with format validation and round-trip support.
- Native tests covering defaults, serialization, escaping, compatibility markers, and parameter validation.

## Milestone 5 — Simplified four-operator Digitone engine ✅

Implement a browser-preview engine that approximates Digitone II FM Tone behaviour.

Delivered:

- Four independent operators.
- Eight four-operator FM topologies represented explicitly by the engine.
- Digitone-style frequency ratios with harmonic shaping.
- Feedback and detune controls.
- Separate A/B runtime envelopes with delay, attack, sustain/triggered decay, and end level.
- Carrier/output mix.
- Independent `DigitoneEngine` kept separate from `DxEngine` and `DigitonePatch`.
- `WebxedSession` preview-engine selection through the thin WASM C ABI.
- Browser DX/Digitone A/B audition controls.
- Native regression tests covering all eight algorithms, finite/bounded output, note release, and audible parameter changes.

Deliverable: hand-authored target patches playable in the browser.

## Milestone 6 — First DX → Digitone converter

Implement a deterministic first-pass converter.

```text
DxPatch
  ↓
DxDigitoneConverter
  ↓
DigitonePatch
```

Keep conversion stages encapsulated as converter members, for example:

```text
analyseAlgorithm()
selectOperators()
selectDigitoneAlgorithm()
mapRatios()
mapLevels()
mapFeedback()
mapEnvelopes()
normaliseOutput()
```

### Operator reduction

Reduce six DX operators to four.

Initial heuristic:

1. Identify carriers.
2. Preserve important carriers.
3. Determine modulation chains.
4. Score modulators by contribution.
5. Discard the lowest-value operators.
6. Preserve the feedback operator where possible.

Useful score inputs include output level, topology role, carrier distance, feedback participation, envelope energy, and velocity sensitivity.

### Conversion report

Return metadata describing lossy decisions, for example:

```text
ConversionReport {
    removedOperators
    mergedOperators
    selectedAlgorithm
    warnings
}
```

### Algorithm conversion

Do not number-map 32 DX algorithms to 8 target algorithms. Treat them as graphs:

```text
DX topology
↓
remove selected operators
↓
normalise graph
↓
closest Digitone topology
```

### Ratio conversion

Map surviving frequencies to the nearest target ratios while preserving important frequency relationships. Record approximation error.

### Envelope conversion

Use a dedicated envelope mapper rather than mapping raw DX rate/level numbers directly.

Conceptually:

```text
DX envelope parameters
↓
evaluate amplitude curve over time
↓
identify attack / peak / decay / terminal behaviour
↓
fit target envelope
```

Numerical fitting can be improved later.

## Milestone 7 — Main A/B workflow

This is the key product milestone.

Target interaction:

```text
random internet .syx
        ↓
browse
        ↓
DX preview
        ↓
convert
        ↓
Digitone preview
        ↓
tweak
        ↓
save
```

Main screen should show source and target side by side, including:

- Source patch name and DX algorithm.
- Target patch name and selected Digitone algorithm.
- Conversion warnings.
- Instant DX/DN A/B switching.
- Convert, Edit, and Save actions.

Suggested shortcuts:

- Left / Right — previous / next patch.
- D — source DX preview.
- N — target preview.
- C — convert.
- Space — audition/replay.
- E — edit.
- S — save.

Deliverable: the complete useful Webxed loop.

## Milestone 8 — Digitone editing UI

Add a focused editor for the converted target patch.

Controls should mirror the conceptual Digitone workflow, not attempt a pixel-perfect hardware skin.

Initial controls:

- Algorithm.
- C / A / B1 / B2 ratios.
- HARM.
- DETUNE.
- FEEDBACK.
- MIX.
- Operator envelopes.

All edits should update the browser preview live.

## Milestone 9 — Saving and local library

Use a Webxed-owned format first rather than attempting undocumented native Digitone preset export.

Example:

```json
{
  "format": "webxed-digitone",
  "version": 1,
  "name": "E Piano 4 DN",
  "source": {
    "bank": "ROM1A",
    "patch": "E.PIANO 4"
  },
  "patch": {}
}
```

Preserve useful provenance:

- Source patch/bank.
- Source SysEx or source hash.
- Converter version.
- Manual edits.
- Conversion warnings.

Store patches locally in the browser initially.

## Milestone 10 — Conversion quality improvements

Improve conversion after testing many real patches.

Potential techniques:

- Spectral/FFT scoring.
- Generate several candidate mappings and choose the closest result.
- Numerical envelope fitting.
- Multiple user-selectable conversion candidates.

## Milestone 11 — Simplified DX editor

Only after the core conversion workflow works well.

Possible modes:

- DX Full — direct DX parameters.
- DX Simple — conventional controls projected onto DX parameters.

A simple ADSR UI must be treated as an approximation of the DX four-rate/four-level envelope, not as a lossless representation.

## Milestone 12 — Hardware transfer assistance

Initially generate a readable hardware recipe rather than relying on an undocumented native preset format.

Example:

```text
E.PIANO 4 DN

FM TONE
ALG     5
RATIO C 1.00
RATIO A 2.00
RATIO B 6.00
HARM    +3
DETUNE  4
FDBK    18
MIX     37
```

Research automatic MIDI/SysEx transfer separately if the native Digitone format is documented and reliable enough.

## Deferred until the main loop works

Do not prioritise these before the DX → Digitone A/B workflow is solid:

- Full Dexed-style editor.
- Filters/effects unrelated to conversion preview.
- Sequencers.
- Elaborate preset management.
- Pixel-perfect Digitone UI recreation.
- Native Digitone preset file output without reliable documentation.

## Current status

- Milestone 1: complete.
- Milestone 2: complete.
- Milestone 3: complete.
- Milestone 4: complete.
- Milestone 5: complete.
- Milestone 6: next.

The highest-value near-term goal remains reaching Milestone 7: browse a real DX bank, preview the source, convert it, preview the Digitone approximation, tweak it, and save it.
