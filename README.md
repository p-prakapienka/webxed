# Webxed

Webxed is a browser-based DX7/Dexed preset audition and Digitone II conversion tool.

Primary workflow:

1. Load a DX7/Dexed SysEx patch or bank.
2. Preview the original patch with a DX-compatible engine.
3. Convert it to a Digitone II-compatible four-operator approximation.
4. Preview and tweak the converted patch using Digitone-style controls.
5. Save the result for later recreation on hardware.

The project intentionally prioritizes the conversion and A/B audition workflow over building a complete Dexed clone.

## Source layout

All application source lives under `src`:

- `src/synth` — synthesis code.
- `src/model` — source and target patch domain models.
- `src/parser` — source format parsers.
- `src/serialization` — versioned Webxed format serializers.
- `src/wasm` — WASM/native bridge.
- `src/web` — browser source.

Build output is generated outside the source tree under `build`.
