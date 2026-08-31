# dn2pst — reverse-engineering the Digitone II sound file format

A single-header C++17 reader for `.dn2pst` files (Elektron Digitone II sound
presets, firmware 1.10E), plus the analysis notes that produced it.

The format is undocumented. Everything here was derived by exporting patches
that differ in exactly one parameter and diffing the bytes. **79 samples** are
included in `samples/` so the whole analysis is reproducible.

## Build

```sh
cmake -B build && cmake --build build
./build/dn2pst -t samples/*.dn2pst
```

Or without CMake:

```sh
g++ -std=c++17 -O2 main.cpp -lz -o dn2pst
```

zlib is the only dependency (the container is a ZIP).

## CLI

```
dn2pst <file...>        full dump: header, name, parameters, trailer
dn2pst -t <file...>     one line per file — the parameter table
dn2pst -m <file...>     8-bit marker / 7-bit block map of the body
dn2pst -d <a> <b>       aligned byte diff of two files
dn2pst -x <file...>     full dump plus a hex dump of the body
dn2pst -u <file...>     EXPERIMENTAL decompression (see caveats in the header)
```

A value printed with a trailing `*` was **not read from the file** — the
parameter has no entry there, so the documented default is reported instead.
That distinction matters: it is the difference between a measurement and an
assumption, and the parser never blurs it.

## What is understood

### Container

A ZIP with two entries: `manifest.json` and a binary payload whose entry name
is the patch name. The manifest carries `FormatVersion`, `FileType`,
`FirmwareVersion` and `Payload`.

### Payload

```
0x00  AC 11 D3 03                magic
0x09  "0050"                     ASCII tag
0x18  <counter>                  decrements once per save
0x22  <seq>                      trailer echoes seq + 8
0x29  BE EF BA CE                section marker
0x2F  <varint> 00 00 00          tracks the name, not any length
      <name>                     13-15 chars, inside the compressed stream
      <body>                     7-bit data blocks split by 8-bit markers
      <trailer>                  crc32-like u32, seq+8, AA A1 DA AA
```

The body follows MIDI's convention: data bytes are 7-bit, and single bytes with
the high bit set act as block markers (`B1 B0 AC F3 FF FF … BA CE F0`, identical
in every sample). All parameter growth lands in the one block after `B1`.

### It is compressed

The payload is LZ77-compressed with a custom codec — not deflate, lzma, lz4,
zstd, lzo, lzf, fastlz, lzjb or classic LZSS, all of which were ruled out by
brute-force sweeps over every start offset.

The proof is a patch named `FM INIT 00000`: that string does not appear in its
own file. It is stored as the literals `FM INIT 0` followed by `01 00 10`, a
back-reference. Every name without a repeated character does appear literally.

**This is why values come and go.** A parameter's value is stored as a literal
byte only when the compressor found no match covering it — so whether a value
is visible depends on the value *and its neighbours*. The cleanest illustration
is the two decays, both default 32:

```
op A decay   op B decay   bytes
    32           32       ... 20 00 7F 10 00 04 08 ...    B's 32 is INVISIBLE
    64           32       ... 40 00 7F 10 00 13 20 08 ... B's 32 is LITERAL
```

Same parameter, same value, present or absent purely according to what preceded
it in the stream.

### Value encodings

Four classes, all verified:

| class | example | encoding |
|---|---|---|
| plain | feedback, levels, envelope stages | literal 0–127 |
| table index | op A / B / C ratios | index into a per-parameter ratio table |
| signed | mix (−64…+63) | stored as value + 64 |
| fixed point | harm, detune | integer + bias, then a 1/256 fraction byte |

Two ratio tables exist. **Op A** has 36 entries (0.25 steps to 5, then 0.5, then
whole numbers); **op C** has 19 (0.25/0.5/0.75/1, then whole numbers). The op B
pair is packed into one number: `packed >> 1 == 19 * b2 + b1`, both using op C's
coarse table.

Fixed-point parameters share the 1/256 fraction and a display that **truncates**
toward zero, but *not* the bias — harm's zero point is 63, detune's is 0. Each
one needs its own bias pinned by a sample.

## What is not understood

- **The match token's length field.** Seven known matches, and `02 00 41`
  encodes both a length-5 and a length-13 match — so the three token bytes
  cannot contain the length. Where it lives is open. The header at `0x2F` is the
  only thing that differs between those two files but is not a plain size.
- **The algorithm value.** Located (it shares a slot with op C ratio) but not
  decoded; it is not the algorithm number.
- **Bit 0 of the packed op B ratio pair**, set in 2 of 8 samples.
- **Page 3** — delay, trig/reset booleans, and the `phrt` enum. Untouched; the
  only remaining unknown *shape*, since everything so far is a number.
- **Filter, amp, FX and mod pages.** Not started. The signed and fixed-point
  rules above should carry over.

## Layout

```
dn2pst.hpp        the reader, and the full format notes as comments
main.cpp          CLI
samples/          79 .dn2pst files, one parameter changed at a time
SAMPLES.md        what each sample file holds
SAMPLES_TODO.md   samples still worth capturing, in priority order
baseline.txt      `dn2pst -t samples/*` output — regression baseline
```

`dn2pst.hpp` is the real documentation. Every finding is recorded there beside
the code that relies on it, including the dead ends, so they do not get
re-explored.

## Adding a parameter

Parameters are located by **anchor pattern search**, never fixed offsets — the
name is variable-length, so every body offset shifts with it. Each parameter is
described by its own entry pattern in `registry()`:

```cpp
{ "op_a_attack", "Operator A attack",
  {{"00 30 00 51 (v) 00 20 00 7F", std::nullopt, "verified: 64, 78, 127"}},
  {},                       // known-undecodable patterns -> reported as unknown
  0, "no entry -> attack 0", {},
}
```

Patterns are hex strings you can paste from a hex editor: `AB` literal, `??`
wildcard, `(v)` capture the value, `(f)` capture a 1/256 fraction.

Resolution is three-step: known-undecodable patterns first (so a bad reading is
never reported as good), then the parameter's own entry, then the documented
default marked as assumed. That ordering is what keeps the tool honest when it
meets a layout it has not seen.

Every new parameter is also a regression test on the existing ones — twice now,
adding one has exposed an over-loose pattern that would have silently misread
another. Re-run `baseline.txt` after any change.
