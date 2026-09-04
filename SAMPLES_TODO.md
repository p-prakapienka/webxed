# Samples still worth capturing

In priority order. Everything listed here is something the current 95 samples
cannot answer. Rules that make a sample usable:

- start from **FM INIT**, change **one** parameter, save under a new name;
- keep the patch name the **same length** across a batch (the name sits inside
  the compressed stream and shifts everything after it);
- say which parameter and which value, including the sign and the decimals as
  the display shows them.

---

## 1. Rework the reader around the known layout — no samples needed

`H162`/`H163` gave the field order and the `<integer> <fraction>` 16-bit shape,
confirmed twice per field. The pattern registry is now understood to be a proxy
for that layout: each `31 <v> 00` or `11 <v>` form is one 16-bit field with its
neighbours matched away. The reader currently reports `?` for both maximal
patches rather than guess, because the sparse-file patterns produce nonsense
against a laid-out record.

The work: read fields by offset from the `28 00 F1 16` anchor for dense records,
keep the pattern registry for sparse ones, and reconcile the two. That is code,
not samples.

## 2. The three things the maximal patches did not settle

| save | what it decides |
|---|---|
| a third maximal patch with a *third* value assignment | whether the field offsets hold, or are an artefact of these two files |
| the same patch with `op A trg` **and** `op B trg` flipped together | the flag bytes at `0x8E`/`0x92` — `0x48` vs `0x46` differs in three bits, so two booleans do not explain them |
| one patch with **only** the four ratio offsets changed, everything else at init | isolates the offsets, which are **not** `integer + n/256` — none of `0x33`, `0x4D`, `0x99`, `0xC0` appears in either maximal patch |

`0x7B` holds 64 in both maximal patches and was set by neither, so it is a
parameter with a default of 64 that is not on FM tone pages 1-4. Worth watching
for once the filter/amp/FX pages are sampled.

## 3. A second sample of any page-3 boolean

`H168`-`H171` located op A/B trig and reset inside the `08 00` group, but each
reading is a whole-group string from a single file - the same evidence shape
that produced the retracted op B decay reading. What breaks or confirms them:

| save | what it decides |
|---|---|
| `op A trg` off **and** `op B trg` off in one patch | whether the two bits share a byte, and whether the single-file forms survive a combination |
| `op A trg` off, saved twice under different names | whether the group is stable for one state or moves with the name |

The maximal patches above partly cover this, since they carry mixed flags.

Also settled already: `0x02`/`0x04` in `10 00 0X 08` did **not** move for op B
delay or for any of the four booleans - only for the op A delay 3-byte form. It
is not a page-3 marker. Twelve page-3 edits agree; no further test needed.

`phrt` is decoded (0 all, 1 off, 2 C, 3 A+B, 4 A+B2, a contiguous index from
zero); only whether a fifth option exists above A+B2 is unknown, and nothing
depends on it.

## 4. Page 4 — key track, and the offsets the maximal patches miss

The maximal patches cover the four ratio offsets and the three key tracks, so
this section only matters if those come back absorbed.

| save | change |
|---|---|
| op A key track **45** | plain 0–127 (range confirmed on the device) |
| op C ratio offset **+0.500** | the exact 1/256 midpoint, if the encoder will land on it |
| op C ratio offset **-0.402** | negative, to pin the bias independently |

Ratio offsets run -1.000 to +0.999, so they never carry an integer part; treat
their whole field as a fraction until a sample says otherwise.

Why a negative sample matters for any new fixed-point parameter: harm and detune
share the 1/256 fraction but *not* the bias (harm's zero point is 63, detune's
is 0). Each one needs its own negative sample to pin its zero point.

## 5. Filter, amp, FX and mod pages

Not started. Same recipe: one parameter per save, and for anything that can go
negative or shows decimals, include one negative and one fractional value.

---

## Open, but not waiting on samples

- **The match token's length field.** `02 00 41` encodes both a length-5 and a
  length-13 match, so the three token bytes cannot hold the length. The seven
  name-based probes in `SAMPLES.md` rule out every simple layout. More names of
  the kind already collected will not help — this needs a different idea rather
  than more data.
- **The algorithm value** — SOLVED by the maximal patches: stored as `alg - 1`
  (7 -> `06`, 4 -> `03`). A third dense patch with a different algorithm would
  make it three points instead of two.
- **Bit 0 of the packed op B ratio pair**, set in 2 of the 8 `BR` samples with
  no visible cause - and set in `H163` (`0x01F8`) but not `H162` (`0x00D9`),
  which swap the same two ratios. Still unexplained.
