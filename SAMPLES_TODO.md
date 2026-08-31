# Samples still worth capturing

Current state: 16 parameters decode across 79 samples. Every value class in the
FM tone is understood (plain, table-indexed, signed, fixed point).

## Conventions that make diffs cheap

- **Keep every name the same length.** Name length shifts every byte after it,
  so two files with equal-length names differ *only* by the parameter change.
  `FM INIT A084` style — prefix, one letter, three zero-padded digits.
- One parameter per file, from a clean FM INIT.
- Record the parameter and value alongside the file; the filename alone is not
  enough once the set grows.

## 1. Page 3 — the last unknown shape

Everything decoded so far is a number. These are the first boolean and the
first named enum, so they will encode unlike anything seen.

- [ ] `op A trg` = **off**
- [ ] `phrt` = **c** (any option other than "all")
- [ ] `op A delay` = 40 (low information — just another 0-127 value)

Suspicion worth testing: bit 0 of the packed op B ratio pair is an unexplained
flag set in 2 of 8 samples. Booleans may live in spare bits like that rather
than taking bytes of their own.

## 2. Page 4 — offsets and key track

- [ ] `op C ratio offset` = +2.00 and -2.00 (fixed point? signed? own bias?)
- [ ] `op A key track` = 64

## 3. Filter / amp / FX / mod pages

Not started. Expect the established rules to carry over, but each signed or
fixed-point parameter needs **its own bias pinned by a sample** — harm biases by
63, detune by 0, mix by 64. Do not assume.

Worth doing first, as the cheapest confirmations:

- [ ] filter cutoff at 3 values
- [ ] one signed parameter (pan, or a mod depth) at min / 0 / max
- [ ] one fractional parameter at a value with a non-zero fraction

## 4. Not worth more effort

**Engineered patch names.** Ten probes are in `samples/`; they established the
match token, minimum match length 4, and that the token does *not* carry the
length. Seven length/distance data points now contradict every simple layout.
Cracking the compressor needs known plaintext in the body, which the parameter
samples already provide.

## Verifying a new batch

```sh
dn2pst -t samples/*.dn2pst        # the table
dn2pst -d fileA fileB             # exact byte differences
dn2pst -m file                    # block structure
ctest --test-dir build            # baseline regression
```

Re-run the baseline after any parser change. Adding a parameter has twice
exposed an over-loose pattern that would have silently misread another one.
