# Samples still worth capturing

In priority order. Everything listed here is something the current 93 samples
cannot answer. Rules that make a sample usable:

- start from **FM INIT**, change **one** parameter, save under a new name;
- keep the patch name the **same length** across a batch (the name sits inside
  the compressed stream and shifts everything after it);
- say which parameter and which value, including the sign and the decimals as
  the display shows them.

---

## 1. Two "maximal" patches — the layout experiment

Everything so far has been one parameter away from init, so the record is mostly
matched and values keep getting absorbed. A patch with **nothing** at its
default has far fewer repeats to match against, so most values land as literals
and the record's **field order** becomes readable — the one thing 93
single-parameter samples still have not given up.

Sizing, from the existing set: one changed parameter adds a mean of +2 bytes and
up to +7 (name-normalised). So ~26 changed parameters should take the payload
from ~235 to roughly **300-380 bytes**, nearly all of it new literals.

### Rules that make it work

- **No value at its default.** That is the whole point.
- **Never 0**, and never 32, 64 or 127 — those are the values that vanish.
- **No two neighbouring parameters equal.** Minimum match length is 4, so a
  single repeated byte cannot form a match; *runs* and repeated multi-byte
  sequences are the enemy. This is why all-off flags would be as bad as
  all-on: init has all four flags on and its page-3 group is the shortest in
  the whole set, that run being swallowed.
- **Alternate the booleans** rather than setting them all one way.
- **14-character names** on both files, so they diff against the FM T0NE set.
- Values are drawn from the 73 byte values that never appear in an init record,
  so every literal is instantly identifiable in a hex dump. That is for
  readability, not compression - single bytes cannot match anyway.

### Why two files rather than a zeroing sweep

Two maximal patches with the same value *set* shuffled between parameters beat
~30 saves zeroing one parameter at a time: each literal carries a different
value in each file, and the pairing identifies which parameter owns it. Then
zero only whatever is still ambiguous.

### The assignment

| parameter | file 1 | file 2 |
|---|---|---|
| op A atk / dec / end / lvl | 23 / 25 / 27 / 29 | 45 / 47 / 41 / 43 |
| op B atk / dec / end / lvl | 31 / 36 / 38 / 41 | 27 / 29 / 23 / 25 |
| feedback / op A delay / op B delay | 42 / 43 / 44 | 38 / 36 / 31 |
| key track A / B1 / B2 | 45 / 46 / 47 | 44 / 42 / 46 |
| mix | -13 | +21 |
| harm / detune | +11.40 / 94.24 | -10.20 / 78.15 |
| alg / op C ratio / op A ratio | 7 / 3.00 / 2.75 | 4 / 8.00 / 1.25 |
| op B1 / B2 ratio | 3.00 / 11.00 | 11.00 / 3.00 |
| op A trg / rst | off / on | on / off |
| op B trg / rst | on / off | off / on |
| phrt | A+B | C |
| ratio offsets C / A / B1 / B2 | +0.50 / +1.25 / -0.50 / +2.00 | -0.50 / +2.00 / +1.25 / +0.50 |

Two things cannot be made collision-free: the booleans are 0/1 by nature, and
the op B ratio pair is packed into one number, so its stored byte is not ours to
choose.

Free side effect: the negative ratio offsets pin the page-4 fixed-point biases,
which otherwise need their own samples (see below).

### What this will and will not give

It gives field **order**, not addresses - positions still shift with anything
upstream of them, which is why both names must be the same length. The
structural framing bytes (`00`, the `11`/`31` field headers) still repeat, so
some matches survive and some values will still be absorbed.

## 2. A second sample of any page-3 boolean

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

## 3. Page 4 — ratio offsets and key track

| save | change |
|---|---|
| op C ratio offset **+0.50** | fixed point, positive |
| op C ratio offset **−0.50** | fixed point, negative — pins the bias |
| op A ratio offset **+2.00** | second fixed-point parameter on the page |
| op A key track **64** | plain 0–127? unverified |

Why the negative one matters: harm and detune share the 1/256 fraction but *not*
the bias (harm's zero point is 63, detune's is 0). Every new fixed-point
parameter needs its own negative sample to pin its zero point — it cannot be
inferred from the others.

## 4. Filter, amp, FX and mod pages

Not started. Same recipe: one parameter per save, and for anything that can go
negative or shows decimals, include one negative and one fractional value.

---

## Open, but not waiting on samples

- **The match token's length field.** `02 00 41` encodes both a length-5 and a
  length-13 match, so the three token bytes cannot hold the length. The seven
  name-based probes in `SAMPLES.md` rule out every simple layout. More names of
  the kind already collected will not help — this needs a different idea rather
  than more data.
- **The algorithm value** — located (it shares a slot with op C ratio), but the
  stored number is not the algorithm number. `H247`–`H249` cover algorithms 2, 4
  and 8; a few more (3, 5, 16) would show whether it is a lookup or an offset, so
  it is worth a batch if convenient.
- **Bit 0 of the packed op B ratio pair**, set in 2 of the 8 `BR` samples with
  no visible cause.
