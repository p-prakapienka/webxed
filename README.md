# dn2pst — reverse-engineering the Digitone II sound file format

A single-header C++17 reader for `.dn2pst` files (Elektron Digitone II sound
presets, firmware 1.10E), plus the analysis notes that produced it.

The format is undocumented. Everything here was derived by exporting patches
that differ in exactly one parameter and diffing the bytes. **95 samples** are
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

Single bytes with the high bit set act as block markers (`B1 B0 AC F3 FF FF …
BA CE F0`, identical in every sample), and all parameter growth lands in the one
block after `B1`. An earlier note here claimed data bytes are 7-bit as in MIDI —
**that is wrong**: the two maximal patches store `0xCB`, `0xD1`, `0xF1`, `0xC5`
and `0xF8` inside the record area, `0xCB` being harm's fraction byte. The
markers are still constant everywhere, so they are probably genuine markers, but
data is not restricted to 7 bits.

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

They also store **floor(value) plus a positive fraction**, not a signed
magnitude. `harm = -4.70` is integer −5 (stored 58 = −5 + 63) with fraction
75/256 = 0.293, giving −4.707, which truncates to −4.70 on the display. One
consequence: no sample yet has a fraction ≥ 0.5, so it is still unknown how a
fraction byte with its high bit set (≥ 0x80) is carried in a 7-bit stream.

### Page 3 is one group, introduced by `08 00`

`08 00` precedes the page-3 delay data in all 93 files, and the byte after it
grows with what the group carries:

```
both delays absorbed   08 00 11 01 18 ...              init, and B delay 64
op A delay explicit    08 00 31 <A> 00 01 48 ...        64, 81, 127
op B delay explicit    08 00 5? 01 00 01 00 <B> 06 ...  14, 127
```

Op A's value comes first in the payload, op B's is the fifth byte. High nibble
of the header = payload byte count fits all five observed shapes (`0x11` → 1,
`0x31` → 3, `0x51`/`0x53` → 5). Take that as a pattern that fits rather than a
proven rule: a framing test over the body showed that almost any byte stream
parses as "nibble length + payload", so the fit carries little independent
weight. The header's low nibble differs between the two op B samples (`0x51`
for 14, `0x53` for 127) and is not decoded.

Op A delay's second payload byte is `0x00` in all three samples. It is probably
a 1/256 fraction, as for harm and detune, but only whole numbers have been
sampled, so the reader matches it as a literal zero rather than claiming a
fraction.

Because the group has a zero-length form, op A delay is one of the few
parameters where "absent" and "present but zero" are actually distinguishable.

### Phase reset, and the `H213` break

Phase reset does **not** live in the `08 00` page-3 group — all four `phrt`
files are byte-identical there. It sits just before op A decay, right after the
`78 3F` detune/feedback anchor:

```
all (H167)     78 3F 22 00 00 3E 00 31 20 00 7F
off (value 1)  78 3F 22 00 11 01 3E 00 31 20 00 7F
C     (H166)   78 3F 22 00 11 02 3E ...
A+B   (H165)   78 3F 22 00 11 03 3E ...
A+B2  (H164)   78 3F 1F 00 11 04 3E ...
```

The field is `11 <value>` followed by `0x3E`, and "all" takes a zero-payload
form. Across all 93 samples the partition is exact: 49 files carry no field, 40
carry value 1 (off), and one each carries 2, 3 and 4. Nothing matches twice.

**This closes the `H213` mystery.** The 49 fieldless files are every save
*before* `H213`; the 40 carrying value 1 are every save from `H213` (BD000)
onward. What was recorded here as an unexplained era marker (`11 01 3E` against
`02 30 00 31`) was simply phase reset holding "off" — switched off at the BD000
save and left off for 45 consecutive saves, which is why it tracked save
order and no parameter. `H167` set it back to "all" and the field vanished
again.

Two consequences: the delay and boolean files all carry `phrt = off`, so they
were never quite init patches; and the earlier localisation was right for the right
reason — LZ matching propagates forward, the divergence sat immediately before
op A decay, and that is exactly where this field is.

The enum, with value 1 named from the device:

| value | option |
|---|---|
| 0 | all — the default, stored as the zero-payload form |
| 1 | off |
| 2 | C |
| 3 | A+B |
| 4 | A+B2 |

A contiguous index from zero — a plain C enum cast to an int, which is what four
directly measured consecutive values say. Whether a fifth option exists above
A+B2 is unknown; nothing in the samples bounds the range.

**One inference, flagged:** `0 = all` is not measured. `H167` stores the
*zero-payload* form, so the byte is absent rather than literally `0x00`. Reading
"no payload" as 0 matches how op A delay behaves and fits an enum starting at
zero, but seeing the literal would need a file with `phrt = all` and the field
emitted explicitly — which, given how 64-style absorption works, may not exist.

So the `H213` story reads: **phase reset was switched off** at the BD000 save and
left off for 45 consecutive saves — through every ratio, envelope, mix, harm,
detune, feedback, delay and boolean sample.

### The record layout

The two "maximal" patches (`H162`, `H163`) — nothing at its default, values
chosen to avoid runs — take the payload to 267 bytes from ~237 and lay the
record out as plain literals. They carry the same value *set* shuffled between
parameters, so every field below is confirmed twice, independently:

| offset | field | H163 | H162 | H163 set | H162 set |
|---|---|---|---|---|---|
| `0x69` | algorithm | `06` | `03` | 7 | 4 |
| `0x6B` | op C ratio index | `05` | `0A` | 3.00 | 8.00 |
| `0x6D` | op A ratio index | `0A` | `04` | 2.75 | 1.25 |
| `0x6F` | op B pair (2 B, BE) | `01 F8` | `00 D9` | 3.00/11.00 | 11.00/3.00 |
| `0x71` | harm — int, frac | `4A 68` | `34 CB` | +11.40 | −10.20 |
| `0x73` | detune — int, frac | `5E 3E` | `4E 28` | 94.24 | 78.15 |
| `0x75` | feedback | `2A` | `26` | 42 | 38 |
| `0x77` | mix | `33` | `55` | −13 | +21 |
| `0x79` | phase reset | `03` | `02` | A+B | C |
| `0x7B` | **??** | `40` | `40` | not set | not set |
| `0x7D`…`0x83` | op A attack, decay, end, level | `17 19 1B 1D` | `2D 2F 29 2B` | 23/25/27/29 | 45/47/41/43 |
| `0x85`…`0x8B` | op B attack, decay, end, level | `1F 24 26 29` | `1B 1D 17 19` | 31/36/38/41 | 27/29/23/25 |
| `0x8D` | op A delay | `2B` | `24` | 43 | 36 |
| `0x8E` | flags — op A trg/rst | `48` | `46` | off/on | on/off |
| `0x91` | op B delay | `2C` | `1F` | 44 | 31 |
| `0x92` | flags — op B trg/rst | `4C` | `4E` | on/off | off/on |
| `0xA4` | key track A, B1, B2 | `2D 2E 2F` | `2C 2A 2E` (+1) | 45/46/47 | 44/42/46 |

**Every field is 16 bits: `<integer> <fraction>`, the fraction in 1/256.** The
run at `0x75`–`0x8D` reads `2A 00 33 00 03 00 40 00 17 00 19 00 …` — integer
parameters carry fraction 0, and harm and detune are the same shape with the
fraction filled in. That one rule explains the `31 <v> 00` and `11 <v>` forms
the pattern registry grew: they are this field with its neighbours matched away.

The op B ratio pair is 16-bit big-endian in one slot, confirming the packing
formula on a much denser sample: `0x01F8` = 504, `504 >> 1` = 252 = 19·13 + 5 →
b1 index 5 (3.00), b2 index 13 (11.00); `H162` gives `0x00D9` = 217 → 19·5 + 13,
the pair swapped.

**The algorithm is stored as `alg − 1`** (7 → `06`, 4 → `03`). Two points, from
files dense enough for the field to be literal; the older `ALG` samples are too
compressed there to confirm independently.

## What is not understood

- **The match token's length field.** Seven known matches, and `02 00 41`
  encodes both a length-5 and a length-13 match — so the three token bytes
  cannot contain the length. Where it lives is open. The header at `0x2F` is the
  only thing that differs between those two files but is not a plain size.
- **Bit 0 of the packed op B ratio pair**, set in 2 of 8 samples — and in `H163`
  (`0x01F8`, even) but not `H162` (`0x00D9`, odd), so still unexplained.
- **`0x7B`** holds 64 in both maximal patches and was set by neither. A
  parameter sitting at a default of 64, located but unidentified — and not on
  FM tone pages 1–4, since those are all accounted for.
- **The two flag bytes** at `0x8E` and `0x92`, immediately after each delay.
  They change with trg/rst, but `0x48` vs `0x46` differs in three bits and
  `0x4C` vs `0x4E` in one, so two booleans do not account for them cleanly.
- **The ratio offsets.** The values are now confirmed from the device screen:
  `H163` = C +.199, A +.301, B1 −.402, B2 +.750; `H162` = C −.199, A +.750,
  B1 +.301, B2 −.402. So the search targets are known, not assumed, and the
  conclusion stands: they are **not** `integer + n/256`, and no single scale
  factor (×100 … ×32768, as u8/u16 LE/BE, signed or magnitude) puts all four in
  a contiguous run anywhere in either file.

  A hard constraint falls out of the displayed values. `.301` is **not
  representable in 1/256** — 77/256 = .300 and 78/256 = .304 — so the ratio
  offsets use a *finer* resolution than every other parameter in the record.
  ×4096 fits all four if the display rounds (815, 1233, −1647, 3072); ×1000
  fits exactly but appears nowhere in either file.

  They should sit in `0x9B`–`0xA4`, between the op B delay flags and key track,
  where `H162` is one byte longer. That region shares no two-byte sequence
  between the files even though both carry `+.301`, `+.750` and `−.402` in
  different slots — which is what a *compressed* region looks like, not a
  laid-out one. Likely blocked on the decompressor rather than on samples.
- **Where op B decay 0 is stored.** `10 00 02 08` was read as op B decay 0 on
  the strength of `H213_FM_INIT_BD000`, its only sample. The three op A delay
  files carry the same three bytes with op B decay at its default 32 — checked
  on the hardware — so that reading is **retracted**. BD000's 0 is real but not
  attributable to any byte, and the reader now reports `?` for it rather than
  the default it would otherwise assume.

  What `0x02`/`0x04` actually is remains open, but the op B delay samples ruled
  out the two obvious readings:

  | file | page-3 field | byte | op B decay |
  |---|---|---|---|
  | init | `11` + 1 byte | `04` | 32 |
  | `H173` B delay 64 | `11` + 1 byte | `04` | 32 |
  | `H174` B delay 14 | `51` + 5 bytes | `04` | 32 |
  | `H172` B delay 127 | `53` + 5 bytes | `04` | 32 |
  | `H175`–`H177` A delay | `31` + 3 bytes | `02` | 32 |
  | `H213` B decay 0 | `11` + 1 byte | `02` | 0 |

  | `H168`–`H171` booleans | varies, 7–11 bytes | `04` | 32 |
  | `H164`–`H167` phrt | `11` + 1 byte | `04` | 32 |

  It is **not** a page-3 group marker — the op B delay and all four boolean
  files change page 3 and keep `04`. It is **not** simply "the page-3 field
  grew" either, or the five-byte files would flip too; only the three-byte form
  does. What is left is an exact but unexplained correlation: `0x02` shows up
  when op A delay is stored explicitly, and in BD000. Twelve page-3 edits across
  four shapes now agree on this, so it is well tested even though unexplained.
- **The page-3 booleans, at byte level.** Both delays are decoded, and op A/B
  trig and reset are *located* — each of `H168`–`H171` produces a distinct
  page-3 group, so the booleans live in the `08 00` group. But every differing
  byte is entangled with the match tokens around it. `op B rst` off differs from
  all-on in exactly two bytes (`18`→`48`, `06`→`04`) with no length change, and
  both of those bytes move for unrelated reasons elsewhere, so neither can be
  called the bit. The reader matches each boolean as a whole-group string from a
  single file and labels the reading **provisional** — that is the same shape of
  evidence that produced the retracted op B decay reading.
- **`phrt` value 1** — the option the patch sat on from `H213` to `H168`. Its
  byte is decoded; only its name on the device is unknown.
- **Filter, amp, FX and mod pages.** Not started. The signed and fixed-point
  rules above should carry over.

## Layout

```
dn2pst.hpp        the reader, and the full format notes as comments
main.cpp          CLI
samples/          95 .dn2pst files, one parameter changed at a time
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
