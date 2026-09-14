# Webxed — agent notes

Browser DX7/Dexed preset audition and Digitone II conversion. Not a Dexed clone.

Read `README.md` and `docs/implementation-plan.md` before changing code. The plan is the product spec, including **Current status** and the next slice. This file is the operating contract: durable rules and how to work. Do not copy milestone accounting here.

Update this file when layout, ABI, commands, or wiring facts change. Update the plan's Current status when a slice lands.

## Not yet wired

`DxDigitoneMapper` exists and is covered by native tests. The product still does not call it:

- `WebxedSession` loads a hand-authored Digitone init voice, not a converted patch.
- The WASM ABI has no convert / report / target-patch exports.
- The browser Digitone button auditions that init voice.

Do not reimplement conversion. Call `DxDigitoneMapper::convert(const DxPatch&)` and keep the source patch unchanged. Delete this section in the same PR that wires convert through session, WASM, and UI.

## Non-negotiables

- Keep `DxPatch` / `DxEngine` separate from `DigitonePatch` / `DigitoneEngine`. Reuse low-level FM parts only where it is clearly cheaper than duplication.
- Conversion is explicit and lossy. Never mutate the source `DxPatch`.
- Do not number-map 32 DX algorithms onto 8 Digitone algorithms. Treat topologies as graphs: reduce operators, normalise, pick the closest of the eight Digitone topologies.
- Keep `WasmBridge.cpp` a thin `extern "C"` ABI over `WebxedSession`. Session and application state do not belong in the bridge.
- Keep `src/web` as classic scripts loaded in dependency order. No ES modules, no bundler. The Emscripten build is `SINGLE_FILE` so `index.html` runs from `file://`.
- Follow the plan's current slice. Do not start work the plan defers until the A/B loop works (full Dexed editor, filters/effects, sequencers, pixel-perfect Digitone skin, native Digitone preset export).
- Dexed is pinned to MSFA only (`DEXED_COMMIT` in `CMakeLists.txt`). Do not pull JUCE or the Dexed application.
- Do not commit `build/`, `build-native/`, `build-web/`, or FetchContent checkouts.

## Layout

All application source is under `src/`:

| Path | Role |
| --- | --- |
| `src/model` | `DxPatch` (156-byte unpacked voice) and hardware-facing `DigitonePatch` |
| `src/synth` | `DxEngine` (MSFA) and `DigitoneEngine` (four-op preview) |
| `src/parser` | DX7 SysEx (single voice and 32-voice packed bank) |
| `src/mapper` | `DxDigitoneMapper` and staged helpers |
| `src/serialization` | Versioned `webxed-digitone` JSON |
| `src/wasm` | `WebxedSession` + thin C ABI |
| `src/web` | Browser shell (classic scripts) |
| `tests/` | Native regression tests, one `main` per binary |

Includes are rooted at `src/`, e.g. `#include "mapper/DxDigitoneMapper.h"`.

## Commands

These match `.github/workflows/ci.yml`. Prefer them over invented cmake flags.

Native tests:

```sh
cmake -S . -B build/test -DCMAKE_BUILD_TYPE=Release
cmake --build build/test --target webxed_tests --config Release
ctest --test-dir build/test --output-on-failure
```

Emscripten web bundle (`build/web/index.html`):

```sh
emcmake cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

C++20, `-Wall -Wextra -Wpedantic`. Native tests are skipped when `EMSCRIPTEN` is set.

## WASM ABI lockstep

Any ABI change must update all three in the same PR:

1. `extern "C"` in `src/wasm/WasmBridge.cpp`
2. `EXPORTED_FUNCTIONS` (and runtime methods) in `CMakeLists.txt`
3. `cwrap` + JS wrapper in `src/web/WebxedApi.js`

Current exports: `_malloc`, `_free`, `_createSynth`, `_destroySynth`, `_loadSysex`, `_patchCount`, `_patchName`, `_selectPatch`, `_selectPreviewEngine`, `_noteOn`, `_noteOff`, `_renderSample`.

`selectPreviewEngine`: `0` = DX, `1` = Digitone. Audition note is A4 (MIDI 69) unless the caller says otherwise.

When wiring conversion, add `WebxedSession` methods first, then one or two ABI functions (convert, report, serialized target). Do not grow a fat C API around mapper internals.

## Conversion pipeline

`DxDigitoneMapper::convert` owns this order. Extend a stage; do not flatten it into one function:

```text
OperatorReducer → AlgorithmMapper → RatioMapper
  → LevelMapper → FeedbackMapper → EnvelopeMapper → normaliseOutput
```

Working state lives in `ConversionContext`. Results are `ConversionResult { DigitonePatch, ConversionReport }`.

## C++ and tests

- C++20, `#pragma once`, small objects with one job, British spelling already used in the mapper (`normalise`).
- `DigitonePatch` holds hardware-facing parameters and ranges (algorithm 1–8, ratios 0.25–16, and so on). DSP-only state stays in `DigitoneEngine`.
- Tests are standalone binaries with a local `expect()` helper and `main()`. Add a function and call it from `main`; do not add gtest/Catch2. Register new binaries on the `webxed_tests` umbrella target.
- Converter tests must keep covering: determinism, source not mutated, all 32 DX algorithms, valid Digitone ranges, finite/bounded audio from a converted patch.

## Browser shell

`AudioEngine`, `PatchBrowser`, `SysexLoader`, `WebxedApi`, `app.js` — keep those responsibilities split.

Keys already in use: Left/Right = previous/next patch, Space and `D` = DX preview, `N` = Digitone preview. The plan's A/B shortcuts also want `C` convert, `E` edit, `S` save; do not steal the existing keys.

`ScriptProcessorNode` is a known temporary audio path; do not treat replacing it as a prerequisite for the A/B loop.

## Pull requests

Match the existing style: Summary, Scope (what this slice does **not** include), Verification (`ctest` and/or the Emscripten build). One plan-shaped change per PR. When a slice lands, mark **Current status** in the implementation plan — not here.
