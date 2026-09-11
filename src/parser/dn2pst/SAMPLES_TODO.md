# Samples still worth capturing

The format is solved: the payload is LZ4, the record is 364 bytes at fixed
offsets, and every FM tone parameter is read directly. What remains is mapping
the parts of the record that have never been touched.

Rules that make a sample usable:

- change **one** parameter, save under a new name;
- say which parameter and what the device screen shows, including sign and
  decimals;
- names no longer need to be the same length — the record is fixed-layout now,
  so a name change shifts nothing.

---

## 1. The other machine pages — filter, amp, FX, modulation

The 364-byte record has roughly 180 bytes past the FM tone block that nothing
has ever written to. Every one of those parameters is a single save away from
being mapped, and each save now pins its offset exactly, with no inference.

Suggested first batch, one parameter per save:

| save | why |
|---|---|
| filter frequency, a mid value | finds where the filter block starts |
| filter resonance | second point in the same block |
| filter envelope depth, **negative** | the first signed field outside page 1 |
| amp attack, decay, sustain, release | four adjacent fields, confirms the block order |
| delay send, reverb send, chorus send | the FX block |
| LFO1 speed, depth, destination | modulation, and `destination` is an enum |

Work outward from there. With the record readable, a batch of ten saves maps
ten offsets with certainty.

## 2. `0x074` and `0x092`

Both are 0 in every save before `H213` and 64 / 127 in every save after. They
changed at the same time as phase reset and have not moved since, so nothing
in the sample set identifies them.

They are **not** on FM tone pages 1–4 — those are fully accounted for. Likely a
sound-level setting (velocity/modulation routing, portamento, etc.). Any save
that moves one of them names it. If a filter/amp sweep happens to change them,
that answers it for free.

## 3. Bit 0 of the packed op B ratio pair

Set in `H163` (`0x01F8`), clear in `H162` (`0x00D9`) — the same two ratios,
swapped. Eight earlier `BR` samples show it set in two of them with no visible
cause. A few saves cycling op B1 alone through adjacent ratios would show
whether it tracks a value or something else.

## 4. The trailer checksum

The u32 at `payload[-12:-8]` is not reproduced by CRC-32, byte sum, word sum, or
Elektron's own OS content checksum, over any start/end range tried. It is the
last part of the container that is not understood. Not blocking anything — the
reader ignores it — but it would be needed to *write* a valid file.

---

## Settled, for the record

- **Compression**: LZ4 block, from payload `0x23`, length at payload `0x19`.
  Exact on all 95 samples.
- **Ratio offsets**: Q1.14, `(raw − 16384) / 16384`, floored to 3 dp.
- **Phase reset**: 0 all, 1 off, 2 C, 3 A+B, 4 A+B2. The firmware lists more
  option strings ("Pitch All", "Pitch A and B2", "Ratio All", "A+B1+B2"), so
  there may be values above 4 — untested, and nothing depends on it.
- **Algorithm**: stored as `alg − 1`.
- **Name probes**: no longer needed. The old plan for non-periodic names was to
  pin the match-length field of an unknown codec; the codec is LZ4 and the
  question is moot.
