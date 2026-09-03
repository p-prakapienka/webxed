# Sample index

93 exports from a Digitone II (firmware 1.10E). Each was made from the **FM INIT**
patch with exactly one parameter changed, so any byte difference between two
files is attributable.

`H164`–`H177` were saved as `FM T0NE …` rather than `FM INIT …`. The name lives
inside the compressed stream and shifts everything after it, so every `FM T0NE`
batch was given names of the same length — which is what reduces each batch's
diff to a single byte.

The `Hnnn` prefix is the export sequence number, descending — it also appears in
the payload at `0x18` as a save counter, which is how that byte was identified.

## FM INIT defaults (from the device)

```
PAGE 1   algorithm 1 | op C ratio 1.00 | op A ratio 1.00
         op B ratios (paired) 1.00 / 1.00 | harm 0.00 | detune 0.00
         feedback 0 | mix -64
PAGE 2   op A attack 0, decay 32, end 127, level 0
         op B attack 0, decay 32, end 127, level 0
PAGE 3   op A delay 0, trig on, reset on | phase reset "all"
         op B delay 0, trig on, reset on
PAGE 4   ratio offsets: op C/A/B1/B2 all 0.00 | key track: op A/B1/B2 all 0
```

Note `decay = 32` and `end = 127`. Assuming those were 0 cost a lot of time —
a parameter sitting at a non-zero default is invisible until you know the
default, because it looks like structure.

## Baseline

| file | change |
|---|---|
| `H256_FM_INIT` | nothing — the reference |
| `H234_FM_INIT_00000` | nothing; re-saved to test determinism. Differs only in counter, name and checksum |

## Operator levels (0–127, default 0)

| file | change |
|---|---|
| `H253_FM_INIT_AL42` | op A level 42 |
| `H255_FM_INIT_AL127` | op A level 127 |
| `H254_FM_INIT_AL64` | op A level 64 — value absorbed into a match |
| `H233_FM_INIT_AL063` | op A level 63 — absorbed |
| `H231_FM_INIT_AL064` | op A level 64, re-saved — reproduces byte for byte |
| `H232_FM_INIT_AL065` | op A level 65 |
| `H225_FM_INIT_AL084` | op A level 84 |
| `H250_FM_INIT_BL84` | op B level 84 |
| `H251_FM_INIT_BL64` | op B level 64 — absorbed |
| `H252_FM_INIT_BL127` | op B level 127 — absorbed |
| `H230_FM_INIT_BL064` | op B level 64, re-saved — reproduces |
| `H229_FM_INIT_BL127` | op B level 127, re-saved — reproduces |

## Envelopes (0–127)

| file | change |
|---|---|
| `H221_FM_INIT_AA001` | op A attack 1 — proves 0x01 is a valid literal |
| `H220_FM_INIT_AA002` | op A attack 2 |
| `H242_FM_INIT_AA64` | op A attack 64 |
| `H241_FM_INIT_AA78` | op A attack 78 |
| `H243_FM_INIT_AA127` | op A attack 127 |
| `H222_FM_INIT_BA048` | op B attack 48 |
| `H223_FM_INIT_BA064` | op B attack 64 — absorbed |
| `H224_FM_INIT_BA127` | op B attack 127 |
| `H238_FM_INIT_AD23` | op A decay 23 (default 32) |
| `H239_FM_INIT_AD64` | op A decay 64 — also makes op B's default 32 visible |
| `H240_FM_INIT_AD127` | op A decay 127 |
| `H213_FM_INIT_BD000` | op B decay 0 — the only sample of this value, and also the save at which phase reset was switched off and left off, so a weak witness |
| `H212_FM_INIT_BD064` | op B decay 64 |
| `H211_FM_INIT_BD127` | op B decay 127 |
| `H237_FM_INIT_AE00` | op A end 0 (default 127) — separates "zero" from "default" |
| `H236_FM_INIT_AE64` | op A end 64 — absorbed |
| `H235_FM_INIT_AE85` | op A end 85 |
| `H208_FM_INIT_BE000` | op B end 0 |
| `H209_FM_INIT_BE046` | op B end 46 |
| `H210_FM_INIT_BE064` | op B end 64 — absorbed |

## Page 3 — delays (0–127, default 0)

| file | change |
|---|---|
| `H175_FM_T0NE_ADL064` | op A delay 64 |
| `H176_FM_T0NE_ADL081` | op A delay 81 |
| `H177_FM_T0NE_ADL127` | op A delay 127 |
| `H174_FM_T0NE_BDL014` | op B delay 14 |
| `H173_FM_T0NE_BDL064` | op B delay 64 — absorbed, nothing literal |
| `H172_FM_T0NE_BDL127` | op B delay 127 |

Within each batch the files differ in exactly one payload byte (beyond name,
save counter and checksum), which pinned both encodings without any inference:

```
init / B delay 64   08 00 11 01 18 ...
A delay  64         08 00 31 40 00 01 48 ...
A delay  81         08 00 31 51 00 01 48 ...
A delay 127         08 00 31 7F 00 01 48 ...
B delay  14         08 00 51 01 00 01 00 0E 06 ...
B delay 127         08 00 53 01 00 01 00 7F 06 ...
```

Both delays live in one group introduced by `08 00` — present in all 93 files —
with op A's value first in the payload and op B's fifth. `08 00 31` matches only
the three op A files; `08 00 ?? 01 00 01 00` only the two readable op B files.
With both delays absorbed the group collapses to `08 00 11 01`.

`H173` (op B delay 64) is byte-identical to an init save in this region apart
from one byte in the token that follows, so its value is not recoverable — the
reader reports `?` for it rather than the 0 default. 64 is the value that gets
absorbed on almost every parameter.

H175 and H176 are byte-identical after the value byte; H177 diverges about 19
bytes later and is a byte longer overall, because `0x7F` occurs elsewhere in the
record and the compressor picked a different match once the value became `0x7F`.

The op A batch also retracted an earlier reading: those three carry
`10 00 02 08`, which had been taken to mean op B decay 0, while the device shows
op B decay at its default 32 in all three. The op B delay batch then ruled out
the two replacement theories as well — see the op B decay note in `README.md`.

## Page 3 — trig and reset booleans (default on)

| file | change |
|---|---|
| `H171_FM_T0NE_ATROFF` | op A trig off |
| `H168_FM_T0NE_ARSOFF` | op A reset off |
| `H170_FM_T0NE_BTROFF` | op B trig off |
| `H169_FM_T0NE_BRSOFF` | op B reset off |

The first non-numeric parameters in the set, and the first the pattern method
cannot really decode. Each file produces a distinct page-3 group, so the
booleans do live in the `08 00` group. Lined up from `08 00`:

```
all on (H173)  11 01 18 00 00 06 00 11     | 7F 60 ...
op A trg off   00 18 00 00 04 00 31 01 00  | 7F 60 ...
op B trg off   13 01 48 00 31 01 00        | 7F 60 ...
op B rst off   11 01 48 00 00 04 00 11     | 7F 60 ...
op A rst off   02 46 00 51 01 00 01 00     | 7F 60 ...
```

`op B rst` off differs from all-on in exactly two bytes (`18`→`48`, `06`→`04`)
with no length change — and both of those bytes move for unrelated reasons
elsewhere, so neither can be called the bit. The reader matches each boolean as
a whole-group string from one file and labels it **provisional**; a second
sample of any of them, or a file with two page-3 edits at once, is expected to
break it.

## Page 3 — phase reset (default "all")

| file | change |
|---|---|
| `H167_FM_T0NE_PHRALL` | phrt "all" — the default. File is named `PHROFF`; the value is "all" |
| `H166_FM_T0NE_PHR__C` | phrt "C" |
| `H165_FM_T0NE_PHRA+B` | phrt "A+B" |
| `H164_FM_T0NE_PHRA+B2` | phrt "A+B2". File is named `PHRA+B1`; the value is A+B2, and the name is 15 chars, not 14 |

Phase reset is **not** in the `08 00` group — all four files are byte-identical
there. It sits just before op A decay, after the `78 3F` anchor:

```
all (H167)     78 3F 22 00 00 3E 00 31 20 00 7F
off (value 1)  78 3F 22 00 11 01 3E 00 31 20 00 7F
C     (H166)   78 3F 22 00 11 02 3E ...
A+B   (H165)   78 3F 22 00 11 03 3E ...
A+B2  (H164)   78 3F 1F 00 11 04 3E ...
```

`H166` and `H165` differ from each other in exactly one payload byte, which is
what pinned it. The enum is a contiguous index from zero: 0 all (zero-payload
form), 1 off, 2 C, 3 A+B, 4 A+B2. Value 1 = off comes from the device; the other
three are measured. `0 = all` is an inference — `H167` stores no payload rather
than a literal `0x00`.

### The `H213` break — solved

Across all 93 files the phrt field partitions exactly: 49 files carry no field,
40 carry value 1 (off), one each carries 2, 3 and 4. The 49 are every save before
`H213`; the 40 are every save from `H213` (BD000) onward.

So the unexplained "change of shape" at `H213` was phase reset being switched
**off** and left off for 45 consecutive saves — which is why it tracked save
order and no parameter. `H167` set it back to "all" and the field vanished.

Every delay and boolean file therefore carries `phrt = off`: they were never
quite init patches.

`01 18` vs `01 48` was reported as a second marker of the same break. It is not:
`H173` is post-break and carries `01 18`, `H169` carries `01 48`, `H172` carries
neither. That byte is a local match choice.

## Ratios

Op A and op C use **different** tables — op A has 36 entries, op C has 19.
1.25 and 2.75 exist only in the fine table, which is what proved they differ.

| file | change |
|---|---|
| `H246_FM_INIT_CR025` | op C ratio 0.25 (index 0) |
| `H228_FM_INIT_CR050` | op C ratio 0.50 (index 1) — never stored as a literal |
| `H227_FM_INIT_CR075` | op C ratio 0.75 — predicted `10 02` before the file existed |
| `H226_FM_INIT_CR200` | op C ratio 2.00 — predicted `10 04` |
| `H244_FM_INIT_CR90` | op C ratio 9.00 |
| `H245_FM_INIT_CR160` | op C ratio 16.00 |
| `H207_FM_INIT_AR025` | op A ratio 0.25 |
| `H206_FM_INIT_AR075` | op A ratio 0.75 |
| `H205_FM_INIT_AR125` | op A ratio 1.25 |
| `H204_FM_INIT_AR275` | op A ratio 2.75 |
| `H203_FM_INIT_AR800` | op A ratio 8.00 |
| `H202_FM_INIT_AR160` | op A ratio 16.00 |

### Op B paired ratios

Both halves ride in one number: `packed >> 1 == 19 * b2 + b1`.

| file | b1 / b2 |
|---|---|
| `H188_FM_INIT_BR2525` | 0.25 / 0.25 |
| `H187_FM_INIT_BR2005` | 2.00 / 0.50 |
| `H186_FM_INIT_BR1675` | 16.00 / 0.75 |
| `H185_FM_INIT_BR111` | 11.00 / 1.00 |
| `H184_FM_INIT_BR056` | 0.50 / 6.00 |
| `H183_FM_INIT_BR1613` | 16.00 / 13.00 |
| `H182_FM_INIT_BR1616` | 16.00 / 16.00 |

## Page 1 miscellany

| file | change |
|---|---|
| `H189_FM_INIT_FB035` | feedback 35 |
| `H190_FM_INIT_FB064` | feedback 64 — absorbed |
| `H191_FM_INIT_FB127` | feedback 127 |
| `H199_FM_INIT_MX20` | mix −20 — first signed parameter |
| `H201_FM_INIT_MX000` | mix 0 |
| `H200_FM_INIT_MX063` | mix +63 |
| `H195_FM_INIT_HR26` | harm −26.00 — first fixed-point parameter |
| `H198_FM_INIT_HR260` | harm +26.00 |
| `H197_FM_INIT_HR716` | harm +7.16 — fractional |
| `H196_FM_INIT_HR47` | harm −4.70 — fractional, negative |
| `H192_FM_INIT_DT6400` | detune 64.00 |
| `H194_FM_INIT_DT127` | detune 127.00 |
| `H193_FM_INIT_DT9424` | detune 94.24 — fractional |
| `H247_FM_INIT_ALG8` | algorithm 8 |
| `H248_FM_INIT_ALG4` | algorithm 4 |
| `H249_FM_INIT_ALG2` | algorithm 2 |

## Compression probes

Init patch, untouched — **only the name varies**. These exist to attack the
compressor with known plaintext, since the name is the one thing whose
decompressed value is known in advance.

| file | name | result |
|---|---|---|
| `H215_FM_INIT_XYZQW` | `FM INIT XYZQW` | no repeats — fully literal, the control |
| `H214_FM_INIT_AAAAB` | `FM INIT AAAAB` | run of 3 — literal, so min match length > 3 |
| `H216_FM_INIT_ABCABC` | `FM INIT ABCABC` | length 3 at distance 3 — literal |
| `H219_FM_INIT_AAAAA` | `FM INIT AAAAA` | distance 1, length 4 → `01 00 10` |
| `H181_FM_INIT_AAAAAA` | `FM INIT AAAAAA` | distance 1, length 5 → `01 00 51` |
| `H218_FM_INIT_AAAAAAA` | `FM INIT AAAAAAA` | distance 1, length 6 → `01 00 31` |
| `H217_FM_INIT_ABABAB` | `FM INIT ABABAB` | distance 2, length 4 → `02 00 51` |
| `H180_FM_INIT_ABABABA` | `FM INIT ABABABA` | distance 2, length 5 → `02 00 41` |
| `H179_AAAAAAAAAAAAAAA` | `AAAAAAAAAAAAAAA` | distance 1, length 14 → `01 00 41` |
| `H178_ABABABABABABABA` | `ABABABABABABABA` | distance 2, length 13 → `02 00 41` |

The last two are the important ones. `02 00 41` encodes **both** a length-5 and
a length-13 match, so the token cannot carry the length — which killed every
model built on that assumption. Minimum match length is 4, confirmed by
`AAAAB` staying literal while `AAAAA` compresses.

Together these probes are exhausted: seven data points contradict every simple
layout. Further progress needs known plaintext inside the body, not the name.
