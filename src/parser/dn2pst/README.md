# dn2pst — reading the Digitone II sound file format

A single-header C++17 reader for `.dn2pst` files (Elektron Digitone II sound
presets, firmware 1.10E), plus the 95 samples the analysis was derived from.

The format is undocumented. It was worked out by exporting patches that differ
in exactly one parameter and diffing the bytes, and the result was then
cross-checked against the device's own firmware image.

## Build

```sh
cmake -B build && cmake --build build && (cd build && ctest)
```

or `g++ -std=c++17 -O2 main.cpp -lz -o dn2pst`. zlib is the only dependency
(the container is a ZIP).

## CLI

```
dn2pst <file...>      full dump of every parameter
dn2pst -t <file...>   one line per file — the regression table
dn2pst -x <file...>   dump plus a hex dump of the decompressed record
dn2pst -r <file...>   write the decompressed record to <file>.rec
```

## The format

### Container

A ZIP with two entries: `manifest.json`, and a binary payload whose ZIP entry
name is the patch name.

```
0x00  AC 11 D3 03     magic
0x04  02 / 00 05 / 00 0F / "0050"   fixed constants
0x0D  u32 BE = 3      u32 BE = 2
0x15  u32 BE          save counter, decrements once per save
0x19  u32 BE          uncompressed record length (364 for a sound)
0x1D  01, 0C
0x23  …               LZ4 block stream
tail  u32 checksum, 00 00 00, seq+8, AA A1 DA AA
```

### It is LZ4

**The payload is plain LZ4 block format.** Decompress from `0x23` for exactly
the length at `0x19`. Verified byte-exact on all 95 samples.

The firmware confirms it: the OS image contains the C++ symbols
`Lz4StreamCompressor` / `Lz4StreamDecompressor` and the LZ4 frame error strings
`ERROR_compressionLevel_invalid` / `ERROR_decompressionFailed`.

### The record

364 bytes, fixed layout, every parameter at a constant offset:

```
0x005  BE EF BA CE     storage-blob start magic
0x011  char[16]        patch name, NUL padded
0x062  …               parameters
0x168  BA CE F0 0C     storage-blob end magic
```

| offset | field | | offset | field |
|---|---|---|---|---|
| `0x062` | algorithm (`alg − 1`) | | `0x07E`…`0x084` | op B attack, decay, end, level |
| `0x064` | op C ratio index | | `0x086` | op A delay |
| `0x066` | op A ratio index | | `0x088` / `0x08A` | op A trig / reset |
| `0x069` | op B ratio pair (packed) | | `0x08C` | op B delay |
| `0x06B` / `0x06C` | harm int / fraction | | `0x08E` / `0x090` | op B trig / reset |
| `0x06D` / `0x06E` | detune int / fraction | | `0x0A5`…`0x0AB` | ratio offsets C, A, B1, B2 |
| `0x06F` | feedback | | `0x0AD`/`0x0AF`/`0x0B1` | key track A, B1, B2 |
| `0x070` | mix (`value + 64`) | | `0x074` | **unidentified** |
| `0x072` | phase reset | | `0x092` | **unidentified** |
| `0x076`…`0x07C` | op A attack, decay, end, level | | | |

### Value encodings

| class | encoding |
|---|---|
| plain | u16 BE, 0–127 |
| algorithm | u16 BE, stored as `alg − 1` |
| ratio | u16 BE index into a per-parameter table (op A 36 entries, op C 19) |
| op B pair | u16 BE at an **odd** offset; `packed >> 1 == 19·b2 + b1`, both into op C's table. Bit 0 is unexplained |
| harm / detune | u8 integer + u8 1/256 fraction; harm's integer is biased by 63, detune's by 0. Display **truncates toward zero** |
| mix | u16 BE, `value + 64` |
| ratio offset | u16 BE at an **odd** offset, Q1.14: `(raw − 16384) / 16384`, so raw 0 = −1.000 and raw 32767 = +0.999. Display **floors** to 3 dp |
| phase reset | u16 BE enum: 0 all, 1 off, 2 C, 3 A+B, 4 A+B2 |

The two rounding rules really do differ, and both are measured against the
device screen rather than assumed.

## Cross-check against the firmware

The layout above was recovered from samples. It was then checked against the
Digitone II OS image (1.10D), decompressed with
[elektron-firmware-tool](https://github.com/mischa85/elektron-firmware-tool)
(clean-room; ELE3 container, all checksums and the HMAC-SHA256 trailer verify).
Three things matched independently:

- the firmware's own FM tone parameter table lists the parameters in the same
  order, and confirms "Ratio B" is a single parameter — hence the packed pair;
- the ratio-offset display format strings (`"+0."` `"-0."` `"-1.000"` `"0.000"`
  `"%s%03d"`) match the Q1.14 reading;
- the header-builder function's immediates are exactly the constants at payload
  `0x00`–`0x1E`.

The firmware image and its extracted sections are **not** part of this
repository. Only the factual structures needed to read the file format are
recorded.

## What is still unknown

- **`0x074` and `0x092`.** Both are 0 in every save before `H213` and 64 / 127
  in every save after. They changed at the same moment as phase reset and have
  never been moved since, so nothing identifies them. They are not on FM tone
  pages 1–4, which are fully accounted for.
- **Bit 0 of the packed op B ratio pair.** Set in `H163` (`0x01F8`) but not in
  `H162` (`0x00D9`), which carry the same two ratios swapped.
- **The trailer checksum.** Not reproduced by CRC-32, byte sum, word sum, or
  Elektron's own OS content checksum over any offset range tried.
- **Everything outside the FM tone machine** — filter, amp, FX and modulation
  live in the same 364-byte record but have not been sampled, so their offsets
  are unassigned.

## Layout

```
dn2pst.hpp        the reader, and the format notes
main.cpp          CLI
samples/          95 .dn2pst files, one parameter changed at a time
SAMPLES.md        what each sample holds
SAMPLES_TODO.md   what is still worth capturing
baseline.txt      `dn2pst -t samples/*` — regression baseline, checked by ctest
```

## How this was done, and what it cost

The first 90 samples were read by pattern-matching anchors in the *compressed*
bytes, because the compression was unidentified. That method worked well enough
to recover most parameters, but it produced readings that later had to be
retracted — most notably `10 00 02 08` as "op B decay 0", which was fitted to
the only sample of that value and contradicted three files later. Values equal
to 64 were routinely invisible, because the compressor matched them away.

Two things ended it. Two "maximal" patches — nothing at its default, values
chosen to avoid runs — made the record lay itself out and gave the field order.
Then the firmware image named the codec. Once LZ4 was identified the pattern
machinery was deleted outright: every value now comes from a fixed offset in the
decompressed record, including the ones that were previously unreadable.
