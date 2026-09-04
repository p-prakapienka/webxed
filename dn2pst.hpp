// dn2pst.hpp — single-header reader for ".dn2pst" sound files.
//
//   #include "dn2pst.hpp"     // requires C++17 and zlib (-lz)
//
// ---------------------------------------------------------------------------
// FORMAT NOTES (reverse-engineered; only what has been empirically verified)
// ---------------------------------------------------------------------------
// A .dn2pst file is a ZIP archive with two entries:
//
//   manifest.json   {"FormatVersion","ProductType","Payload","FileType",
//                    "FirmwareVersion","MetaInfo"}   — "Payload" names entry 2
//   <payload>       the binary patch, entry name == manifest "Payload"
//
// Payload layout (offsets are absolute in the payload, firmware 1.10E):
//
//   0x00  AC 11 D3 03            magic
//   0x04  02 00 05 00            unknown (constant across all samples)
//   0x08  0F
//   0x09  "0050" 00 00 00        ASCII tag, NUL padded
//   0x10  03 00 00 00
//   0x14  02 00 00 07
//   0x18  <counter>              decrements once per save (FF, FE, FD, FC ...)
//   0x19  00 00 01
//   0x1C  6C 01 0C 00 00 00
//   0x22  <seq> 91 00 00 00      <seq> tracks the save; trailer echoes <seq>+8
//   0x27  02 00
//   0x29  BE EF BA CE            section marker
//   0x2D  09 00
//   0x2F  <varint-ish, 1-2 B>    tracks NAME LENGTH, not body length:
//                                7 ch -> B6 | 12 ch -> F1 01 | 13 ch -> F0 02
//         00 00 00               padding
//         <name> 00              NUL-terminated ASCII, VARIABLE length
//   body: parameter stream, byte-aligned and position-independent
//   tail: <crc32-like u32> 00 00 00 <seq+8> AA A1 DA AA
//
// Because the name is variable-length, every body offset shifts with it — so
// parameters are located by ANCHOR PATTERN SEARCH, never by fixed offset.
// The payload IS compressed — with a custom LZ77, not any stock codec (deflate,
// lzma, lz4, zstd, lzo, lzf, fastlz, lzjb and classic LZSS were all ruled out).
// See "EXPERIMENTAL: LZ decompression" below for the match token and the proof.
// Everything the parameter registry does still works on the COMPRESSED bytes,
// which is why a value is sometimes a literal and sometimes not.
//
// OP A ATTACK — the cleanest parameter so far. Between the unique "78 3F" and
// the unique "20 00 7F":
//
//   attack   0   ?? 00 02 30 00 31              (no entry)
//   attack  64   ?? 00 00 30 00 51 40 00        value stored LITERALLY
//   attack  78   ?? 00 00 30 00 51 4E 00
//   attack 127   ?? 00 00 30 00 51 7F 00
//
// AA64 and AA78 differ in exactly ONE body byte (body+48), plus the save
// counter, the two name characters and the checksum. Nothing else moves.
//
// 64 stores as a plain 0x40 here, so there is no globally escaped or reserved
// byte value — see "WHAT STORES EXACTLY" below for where 64 does go wrong.
//
// The byte after "30 00" is a flags/type field, not a checksum:
//   0x31  nothing set        0x51  attack set (+0x20)
//   0x53  op A level set (+0x02) — so 0x20 and 0x02 look like presence bits.
//
// ---------------------------------------------------------------------------
// THE RECORD LAYOUT, from the two "maximal" patches (H162, H163)
// ---------------------------------------------------------------------------
// Every earlier sample was one parameter away from init, so the record was
// mostly matched and values kept vanishing. H163/H162 have NOTHING at its
// default, with distinct values chosen to avoid runs - and the record lays
// itself out. 267 bytes against ~237, and the parameters appear in order as
// plain literals. The two files carry the same value SET shuffled between
// parameters, so every field is confirmed twice, independently:
//
//   off    field                 H163      H162      H163 set     H162 set
//   0x69   algorithm             06        03        7            4
//   0x6B   op C ratio index      05        0A        3.00         8.00
//   0x6D   op A ratio index      0A        04        2.75         1.25
//   0x6F   op B pair, 2B BE      01 F8     00 D9     3.00/11.00   11.00/3.00
//   0x71   harm    int, frac     4A 68     34 CB     +11.40       -10.20
//   0x73   detune  int, frac     5E 3E     4E 28     94.24        78.15
//   0x75   feedback              2A        26        42           38
//   0x77   mix                   33        55        -13          +21
//   0x79   phase reset           03        02        A+B          C
//   0x7B   ??                    40        40        (not set)    (not set)
//   0x7D   op A attack           17        2D        23           45
//   0x7F   op A decay            19        2F        25           47
//   0x81   op A end              1B        29        27           41
//   0x83   op A level            1D        2B        29           43
//   0x85   op B attack           1F        1B        31           27
//   0x87   op B decay            24        1D        36           29
//   0x89   op B end              26        17        38           23
//   0x8B   op B level            29        19        41           25
//   0x8D   op A delay            2B        24        43           36
//   0x8E   flags (A trg/rst)     48        46        off/on       on/off
//   0x91   op B delay            2C        1F        44           31
//   0x92   flags (B trg/rst)     4C        4E        on/off       off/on
//   0xA4   key track A/B1/B2     2D 2E 2F  (+1)      45/46/47     44/42/46
//
// EVERY FIELD IS 16 BITS: <integer> <fraction>, the fraction in 1/256. The run
// at 0x75..0x8D reads "2A 00 33 00 03 00 40 00 17 00 19 00 ..." - integer
// parameters simply carry fraction 0, and harm and detune are the same shape
// with the fraction filled in (68 = 0.406, 3E = 0.242). That single rule
// explains the "31 <v> 00" and "11 <v>" forms the pattern registry below
// grew: they are this field with its neighbours matched away.
//
// The op B ratio pair is 16-bit BIG-ENDIAN in the same slot: 0x01F8 = 504,
// 504 >> 1 = 252 = 19*13 + 5, i.e. b1 index 5 (3.00) and b2 index 13 (11.00).
// H162 gives 0x00D9 = 217, 217 >> 1 = 108 = 19*5 + 13, the pair swapped.
// Confirms the packing formula on a second, much denser sample.
//
// ALGORITHM: stored as alg - 1. 7 -> 06 and 4 -> 03. Two points, from files
// dense enough for the field to be literal at a known offset; the older ALG
// samples are too compressed there to confirm independently.
//
// CORRECTION to the "7-bit data, 8-bit markers" note below: H162 stores harm's
// fraction as 0xCB (203) at 0x72, and 0xD1, 0xF1, 0xC5, 0xF8 all appear inside
// the record area of these two files. Data bytes are NOT restricted to 7 bits.
// The high-bit bytes listed as block markers are still constant across every
// sample, so they are probably real markers, but "all data is 7-bit" is wrong.
//
// STILL OPEN after these two files:
//   - 0x7B holds 64 in both files and was never set by either. A parameter at
//     a default of 64, located but unidentified - not on FM tone pages 1-4.
//   - the flag bytes at 0x8E and 0x92 sit immediately after each delay and
//     change with trg/rst, but 0x48 vs 0x46 differs in three bits and 0x4C vs
//     0x4E in one, so two booleans do not account for them cleanly. A patch
//     with two flags changed at once is the test.
//   - the ratio offsets are NOT integer + n/256: none of the expected bytes
//     (0x33, 0x4D, 0x99, 0xC0) appear anywhere in either file. They must live
//     in 0x9B..0xA4, between the op B delay flags and key track, where H162 is
//     one byte longer than H163. Encoding unknown.
//
// OP A DECAY — always present, always literal. THE INIT DEFAULT IS 32, NOT 0.
// The byte before the FIRST "00 7F" in the body holds it:
//
//   decay  23   ... 31 17 00 7F ...      decay  64   ... 31 40 00 7F ...
//   decay  32   ... 31 20 00 7F ...      decay 127   ... 31 7F 00 7F ...
//
// 32 in all 33 files where decay was untouched, and the edited value in the
// three AD files. No exceptions across 36 samples.
//
// IMPORTANT CONSEQUENCE: "20 00 7F", which the notes below still call an anchor,
// is NOT structure — the 0x20 is the decay value 32. It vanished in the AD files
// because the value changed, not because the layout moved. Every pattern keyed
// to it is really keyed to "decay is at its default", and should be re-derived
// once more init defaults are known.
//
// OP B DECAY — the clearest demonstration of the compression at work. Also
// default 32, and it sits four bytes after op A decay:
//
//   op A decay   op B decay   bytes
//   ----------   ----------   ------------------------------
//       32           32       ... 20 00 7F 10 00 04 08 ...    B's 32 INVISIBLE
//       64           32       ... 40 00 7F 10 00 13 20 08 ... B's 32 is LITERAL
//       32           64       ... 20 00 7F 10 00 13 40 08 ...
//       32          127       ... 20 00 7F 10 00 13 7F 08 ...
//       32            0       ... 20 00 7F 10 00 02 08 ...    RETRACTED
//
// THE LAST ROW IS WRONG. H175-H177 carry the same "10 00 02 08" and the device
// shows op B decay at its default 32 in all three. 0x02 therefore does not
// encode "op B decay 0"; the value 0 in BD000 is not attributable to any byte
// yet. See the op_b_decay entry in the registry for what 0x02/0x04 probably
// are instead (a match length) and for how to test it.
//
// Worth keeping in mind when reading the table above: BD000 is also the save
// at which PHASE RESET was changed and left changed. Every save from H213
// onward carries phrt = off (1); every earlier save has no phrt field at all.
// That is what the "11 01 3E" / "02 30 00 31" era markers were - see the
// phase_reset entry in the registry. So BD000 is a poor witness for anything,
// being both the first file of the new phrt state and the only sample of op B
// decay 0.
//
// The way that was localised is worth reusing. Sounds are device-wide on this
// machine rather than per project, so a project switch could not account for
// it. And LZ matching propagates forward only: a change at logical position P
// can alter compressed output from P onward, not before it (bar a byte or two
// of lazy-match lookahead). The divergence point sat immediately before
// "00 31 20 00 7F" - op A decay 32.00, then op A end 127 - which put the
// changed field EARLIER in the record than op A decay. phase_reset turned out
// to be exactly there.
//
// CORRECTION on the way: "01 18" vs "01 48" was reported as a second marker of
// the same break. It is not - H173 is a post-break file carrying "01 18", and
// H169 carries "01 48" while H172 carries neither. That byte is a local match
// choice.
//
// In init BOTH decays are 32, and op B's copy simply is not in the file — the
// identical 0x20 a few bytes earlier (op A's) absorbs it into a match. Change op
// A decay to 64 and op B's 32 immediately appears as a literal 0x20, untouched.
// Same parameter, same value, present or absent purely according to what came
// before it. That is the whole "missing value" phenomenon in one table.
//
// This also reopens the "omitted value" rule. A parameter looked absent whenever
// its value equalled a default I had assumed was 0. Decay shows a parameter at
// its default sitting in plain sight, stored literally. The record may simply be
// a list of literal values with no omission rule at all.
//
// OP A END — the parameter that separates the two "omit" rules. Its default is
// 127, not 0:
//
//   end 127 (default)  ?? 00 02 30 00 31 20 00 7F 10 00 04    no entry
//   end   0            ?? 00 0A 30 00 31 20 00 7F 18 00 11    no value byte
//   end  64            ?? 00 04 30 00 02 44 00 31 ...         0x44 = 68, NOT 64
//   end  85            ?? 00 04 30 00 11 55 10 00 31 ...      0x55 = 85, literal
//
// end = 0 carries no value byte even though 0 is not this parameter's default,
// and end = 127 (which IS the default) also carries none. But see the DECAY
// note below: decay sits in the file literally even at its default, so
// "omitted" may just mean "I did not know the default and so did not
// recognise the value".
//
// WHAT STORES EXACTLY AND WHAT DOES NOT (all samples so far):
//
//   parameter      exact                        not exact
//   -----------    --------------------------   -----------------------
//   op A attack    64, 78, 127                  —
//   op A decay     23, 64, 127                  —
//   op A level     42, 127                      64 -> 0x46 (+6)
//   op A end       85                           64 -> 0x44 (+4)
//   op B level     84                           64 -> 0x4E (+14), 127 -> 0x0A
//
// RESOLVED by the LZ finding: a value appears literally only when the compressor
// found no match covering it. Matches need several bytes, so whether a value is
// literal depends on the value AND its neighbours — which is why the same value
// behaves differently on different parameters.
//
// 64 (0x40) is the value that most often gets absorbed into a match:
//   exact:      op A attack 64, op A decay 64
//   absorbed:   op A level 64 -> 46, op A end 64 -> 44,
//               op B level 64 -> 4E, op B attack 64 -> 4C
// In every absorbed case the presence-flag byte also stays at its "nothing set"
// value (0x31) instead of rising, so the flag byte is part of the same encoded
// unit rather than a separate bitmap.
//
// Values verified as EXACT somewhere: 1, 2, 23, 42, 48, 78, 84, 85, 127.
// op A level 84 -> 00 54, the same literal op B level uses, so the level family
// is not broken as such.
//
// Verified by re-save: the encoder is DETERMINISTIC. op B level 64 and 127 were
// saved a second time and reproduce their odd encodings byte for byte, and a
// no-edit re-save of FM INIT differs only in the save counter, the name and the
// checksum. Nothing here is a save glitch.
//
// PARAMETER RECORD AREA (op A/B level samples)
// Level entries sit immediately after the unique anchor "20 00 7F":
//
//   op A   op B   bytes following the anchor
//   ----   ----   ---------------------------------------------
//      0      0   10 00 04  08 00 11 01 18 ...
//     42      0   00 2A     08 00 00 19 00 11 01 18 ...      value literal
//    127      0   00 7F     08 00 00 19 00 11 01 18 ...      value literal
//     64      0   46 00 00  08 00 00 19 00 11 01 18 ...      NOT literal
//      0     84   10 00 00  08 00 11 54 16 00 00 18 ...      value literal (0x54)
//      0    127   10 00 00  08 00 00 0A 00 11 01 18 ...      NOT literal
//      0     64   10 00 00  08 00 00 4E 00 11 01 18 ...      NOT literal
//
// So a value is NOT simply stored as a byte, and it is not a per-value escape
// either: op A = 127 is literal while op B = 127 is not. Whatever encodes the
// value depends on both the parameter and its value, which is why every form
// below is listed only for the samples that actually verified it, and anything
// else is reported as UnknownEncoding rather than guessed at.
//
// Leading "10 00" means op A is 0 (its 3rd byte tracks op B), and the pair of
// zero-forms for op B cover the cases where op A is non-zero.
//
// ALGORITHM AND OP C RATIO SHARE ONE SLOT, between "02 00 ?? 28 00" and the
// unique "78 3F":
//
//     alg 1, ratio 1.00   01 38 00 23        alg 8   12 07 38 00 23
//     alg 2               03 38 00 23        alg 4   00 36 00 53 03 00 00
//     ratio  9.00         10 0B 38 00 23     ratio 0.25   53 03 00 00
//     ratio 16.00         10 12 38 00 23
//
// The ratio decodes: "10 <index>" where index comes from the ratio table
// 0.25/0.50/0.75 = 0/1/2 and n.00 = n+2. CONFIRMED by prediction: 0.75 and 2.00
// were predicted as 10 02 and 10 04 before the samples arrived, and both came
// back exactly. Verified points are now 0, 2, 3, 4, 11 and 18.
//
// The exception is index 1 (ratio 0.50), stored as 00 3A 00 33 00 with no
// "10 01" anywhere.
//
// CORRECTION: I first read that as "0x00 and 0x01 are reserved bytes that a
// literal can never be". That is WRONG. op A attack = 1 stores as a plain
// literal 0x01 (see AA001), and attack = 2 as 0x02. So 0x01 is both a valid
// literal AND the match opcode, which means the stream must carry flag bits
// somewhere to tell them apart — a bitmap or a header field that has not been
// found yet. A search over token grammars (1- and 2-byte operands, 7..11-bit
// distances, several length biases, with and without a flag bit inside the
// token) produced NOTHING that satisfies the name case and the AA001/AA002 and
// CR075/CR200 one-byte-difference constraints simultaneously.
//
// USEFUL NEXT SAMPLES
//   - op C ratio = 0.50, 0.75 and 2.00 — three points that confirm or break the
//     inferred middle of the ratio index table in one go
//   - op A level = 84 (does the same value encode the same way on a different
//     operator?), and op A = 1, 2, 3 (smallest values)
//   - op B level = 42 and 127 again, saved twice each
//   - FM INIT saved twice with NO edit: if the two bodies differ by more than
//     the save counter at 0x18 and the trailer checksum, the encoder is not
//     deterministic and single-parameter byte diffs cannot be trusted alone
//
// ---------------------------------------------------------------------------
// FM TONE INIT PATCH — every parameter and its default (from the device)
// ---------------------------------------------------------------------------
// This is the reference the whole project was missing. A parameter looked
// "absent" whenever its value happened to equal a default assumed to be 0.
//
//   PAGE 1   algorithm 1 | op C ratio 1.00 | op A ratio 1.00
//            op B ratios (paired) 1.00 / 1.00 | harm (osc shape) 0.00
//            detune 0.00 | feedback 0 | mix (carriers X/Y) -64
//   PAGE 2   op A attack 0, decay 32, end 127, level 0
//            op B attack 0, decay 32, end 127, level 0
//   PAGE 3   op A delay 0, trig on, reset on | phase reset "all"
//            op B delay 0, trig on, reset on
//   PAGE 4   ratio offsets: op C 0.00, op A 0.00, op B1 0.00, op B2 0.00
//            key track: op A 0, op B1 0, op B2 0
//
// (Filter, amp, FX and mod pages are not covered yet.)
//
// WHAT THIS ALREADY SETTLES
//   - op B end default is 127, matching op A end. Confirmed.
//   - Both decays default to 32; both levels and attacks to 0.
//   - Every ratio defaults to 1.00, i.e. index 3 in the ratio table, and init
//     does contain repeated 0x03 bytes early in the record area.
//   - MIX defaults to -64, the first SIGNED parameter in the set. Its range is
//     -64..+63, so it is probably stored offset-encoded (-64 -> 0) or with a
//     sign bit; either way it is the parameter that will show how negatives
//     work, and nothing else in the set can.
//   - Roughly 30 parameters exist for the FM tone, against a record area of
//     ~85 compressed bytes, so most values must be at defaults and absorbed.
//
// WHAT IT CANNOT DO ALONE: pin positions. Because the stream is compressed,
// a parameter's byte moves whenever anything before it changes length, so
// "value X lives at offset N" only holds within one layout. Mapping the full
// record needs the decompressor, not more defaults.
//
// ---------------------------------------------------------------------------
// ADDING A PARAMETER
// ---------------------------------------------------------------------------
// Append one entry to registry() below. Patterns are hex strings you can paste
// straight out of a hex editor:
//
//      AB    literal byte
//      ??    any byte
//      (v)   capture this byte as the parameter value
//
//   { "op_b_level", "Operator B level",
//     "<anchor bytes>",
//     { { "00 (v) ...", std::nullopt, "note" },        // value captured
//       { "<bytes>",    0,            "level 0" } } }  // fixed value
//
// Forms are tried in order; the first match wins. If none match, the decoder
// returns UnknownEncoding plus the raw bytes at the entry position, which is
// exactly what you want when a new sample introduces an encoding.
// ---------------------------------------------------------------------------

#ifndef DN2PST_HPP_INCLUDED
#define DN2PST_HPP_INCLUDED

#include <zlib.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace dn2pst {

using Bytes = std::vector<uint8_t>;

// ===========================================================================
// errors + small helpers
// ===========================================================================

class Error : public std::runtime_error {
 public:
  explicit Error(const std::string& what) : std::runtime_error(what) {}
};

inline std::string toHex(const uint8_t* p, size_t n, const char* sep = " ") {
  static const char* kDigits = "0123456789ABCDEF";
  std::string out;
  out.reserve(n * 3);
  for (size_t i = 0; i < n; ++i) {
    if (i) out += sep;
    out += kDigits[p[i] >> 4];
    out += kDigits[p[i] & 0x0F];
  }
  return out;
}

inline std::string toHex(const Bytes& b, const char* sep = " ") {
  return toHex(b.data(), b.size(), sep);
}

inline Bytes readWholeFile(const std::string& path) {
  std::FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) throw Error("cannot open file: " + path);
  Bytes buf;
  uint8_t chunk[64 * 1024];
  size_t n;
  while ((n = std::fread(chunk, 1, sizeof chunk, f)) > 0)
    buf.insert(buf.end(), chunk, chunk + n);
  const bool bad = std::ferror(f) != 0;
  std::fclose(f);
  if (bad) throw Error("read error: " + path);
  return buf;
}

inline uint16_t rdU16(const uint8_t* p) {
  return uint16_t(p[0] | (uint16_t(p[1]) << 8));
}
inline uint32_t rdU32(const uint8_t* p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) |
         (uint32_t(p[3]) << 24);
}

// ===========================================================================
// minimal ZIP reader (stored + deflate), enough for .dn2pst containers
// ===========================================================================

struct ZipEntry {
  std::string name;
  Bytes data;
  uint32_t storedCrc32 = 0;
  bool crcOk = false;
};

inline Bytes inflateRaw(const uint8_t* src, size_t srcLen, size_t expected) {
  Bytes out;
  out.resize(expected ? expected : (srcLen * 4 + 64));

  z_stream zs{};
  if (inflateInit2(&zs, -MAX_WBITS) != Z_OK) throw Error("inflateInit2 failed");
  zs.next_in = const_cast<Bytes::value_type*>(src);
  zs.avail_in = static_cast<uInt>(srcLen);

  size_t written = 0;
  int rc = Z_OK;
  for (;;) {
    if (written == out.size()) out.resize(out.size() * 2 + 64);
    zs.next_out = out.data() + written;
    zs.avail_out = static_cast<uInt>(out.size() - written);
    rc = inflate(&zs, Z_NO_FLUSH);
    written = out.size() - zs.avail_out;
    if (rc == Z_STREAM_END) break;
    if (rc == Z_OK || rc == Z_BUF_ERROR) {
      if (zs.avail_in == 0 && rc == Z_BUF_ERROR) break;  // truncated but usable
      continue;
    }
    inflateEnd(&zs);
    throw Error("inflate failed, zlib code " + std::to_string(rc));
  }
  inflateEnd(&zs);
  out.resize(written);
  return out;
}

inline std::vector<ZipEntry> readZip(const Bytes& buf) {
  static constexpr uint32_t kEocd = 0x06054b50;
  static constexpr uint32_t kCdir = 0x02014b50;
  static constexpr uint32_t kLocal = 0x04034b50;

  if (buf.size() < 22) throw Error("not a zip: too small");

  // End of central directory: scan backwards (no zip64, no archive comment
  // longer than 64 KiB — fine for these files).
  size_t eocd = 0;
  bool found = false;
  const size_t limit = buf.size() >= 66000 ? buf.size() - 66000 : 0;
  for (size_t i = buf.size() - 22 + 1; i-- > limit;) {
    if (rdU32(&buf[i]) == kEocd) {
      eocd = i;
      found = true;
      break;
    }
  }
  if (!found) throw Error("not a zip: no end-of-central-directory record");

  const uint16_t count = rdU16(&buf[eocd + 10]);
  const uint32_t cdOffset = rdU32(&buf[eocd + 16]);
  if (cdOffset >= buf.size()) throw Error("zip: bad central directory offset");

  std::vector<ZipEntry> entries;
  size_t p = cdOffset;
  for (uint16_t i = 0; i < count; ++i) {
    if (p + 46 > buf.size() || rdU32(&buf[p]) != kCdir)
      throw Error("zip: bad central directory entry " + std::to_string(i));

    const uint16_t method = rdU16(&buf[p + 10]);
    const uint32_t crc = rdU32(&buf[p + 16]);
    const uint32_t compSize = rdU32(&buf[p + 20]);
    const uint32_t uncompSize = rdU32(&buf[p + 24]);
    const uint16_t nameLen = rdU16(&buf[p + 28]);
    const uint16_t extraLen = rdU16(&buf[p + 30]);
    const uint16_t commentLen = rdU16(&buf[p + 32]);
    const uint32_t localOff = rdU32(&buf[p + 42]);
    if (p + 46 + nameLen > buf.size()) throw Error("zip: truncated entry name");

    ZipEntry e;
    e.name.assign(reinterpret_cast<const char*>(&buf[p + 46]), nameLen);
    e.storedCrc32 = crc;

    if (localOff + 30 > buf.size() || rdU32(&buf[localOff]) != kLocal)
      throw Error("zip: bad local header for " + e.name);
    const uint16_t lNameLen = rdU16(&buf[localOff + 26]);
    const uint16_t lExtraLen = rdU16(&buf[localOff + 28]);
    const size_t dataAt = localOff + 30 + lNameLen + lExtraLen;
    if (dataAt + compSize > buf.size())
      throw Error("zip: truncated data for " + e.name);

    if (method == 0) {
      e.data.assign(buf.begin() + dataAt, buf.begin() + dataAt + compSize);
    } else if (method == 8) {
      e.data = inflateRaw(&buf[dataAt], compSize, uncompSize);
    } else {
      throw Error("zip: unsupported compression method " +
                  std::to_string(method) + " for " + e.name);
    }

    const uint32_t actual = static_cast<uint32_t>(
        ::crc32(::crc32(0L, Z_NULL, 0), e.data.data(),
                static_cast<uInt>(e.data.size())));
    e.crcOk = (actual == crc);

    entries.push_back(std::move(e));
    p += 46 + nameLen + extraLen + commentLen;
  }
  return entries;
}

// ===========================================================================
// manifest.json (tiny string-field extractor — no JSON dependency)
// ===========================================================================

inline std::optional<std::string> jsonStringField(std::string_view json,
                                                  std::string_view key) {
  const std::string needle = "\"" + std::string(key) + "\"";
  size_t k = json.find(needle);
  if (k == std::string_view::npos) return std::nullopt;
  size_t c = json.find(':', k + needle.size());
  if (c == std::string_view::npos) return std::nullopt;
  size_t q = json.find('"', c + 1);
  if (q == std::string_view::npos) return std::nullopt;
  std::string out;
  for (size_t i = q + 1; i < json.size(); ++i) {
    if (json[i] == '\\' && i + 1 < json.size()) {
      out += json[++i];
      continue;
    }
    if (json[i] == '"') return out;
    out += json[i];
  }
  return std::nullopt;
}

struct Manifest {
  std::string formatVersion;
  std::string payload;  // name of the payload zip entry
  std::string fileType;
  std::string firmwareVersion;
  std::string raw;
};

inline Manifest parseManifest(const Bytes& json) {
  Manifest m;
  m.raw.assign(reinterpret_cast<const char*>(json.data()), json.size());
  auto get = [&](const char* k) {
    return jsonStringField(m.raw, k).value_or(std::string{});
  };
  m.formatVersion = get("FormatVersion");
  m.payload = get("Payload");
  m.fileType = get("FileType");
  m.firmwareVersion = get("FirmwareVersion");
  return m;
}

// ===========================================================================
// payload header / trailer
// ===========================================================================

struct Header {
  std::array<uint8_t, 4> magic{};
  std::string tag;            // "0050" @ 0x09
  uint8_t saveCounter = 0;    // 0x18 — decrements once per save
  uint8_t seq = 0;            // 0x22 — trailer echoes seq + 8
  size_t markerOffset = 0;    // BE EF BA CE
  Bytes nameLenField;         // bytes between marker+6 and the name
  bool magicOk = false;
};

struct Trailer {
  uint32_t checksum = 0;  // 4 bytes LE, changes on every save
  uint8_t seqEcho = 0;
  bool magicOk = false;    // AA A1 DA AA
  bool seqEchoOk = false;  // seqEcho == header.seq + 8 in all known samples
};

struct Patch {
  std::string name;   // payload name; equals manifest "Payload"
  Bytes raw;          // whole payload
  size_t nameOffset = 0;
  size_t bodyOffset = 0;  // first byte after the name's NUL
  Header header;
  Trailer trailer;

  const uint8_t* body() const { return raw.data() + bodyOffset; }
  size_t bodySize() const { return raw.size() - bodyOffset; }
};

// ===========================================================================
// pattern DSL:  "23 78 3F ?? 00 (v)"   AB literal | ?? any | (v) capture
// ===========================================================================

struct Tok {
  enum Kind { Lit, Any, Cap, Frac } kind = Lit;  // Frac = "(f)", 1/256 part
  uint8_t byte = 0;
};

inline std::vector<Tok> parsePattern(std::string_view pat) {
  std::vector<Tok> toks;
  size_t i = 0;
  auto hexVal = [](char c) -> int {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
  };
  while (i < pat.size()) {
    const char c = pat[i];
    if (c == ' ' || c == ',' || c == '\t' || c == '\n') {
      ++i;
      continue;
    }
    if (c == '?') {
      if (i + 1 >= pat.size() || pat[i + 1] != '?')
        throw Error("pattern: expected two '?' characters");
      toks.push_back({Tok::Any, 0});
      i += 2;
      continue;
    }
    if (c == '(') {
      const size_t close = pat.find(')', i);
      if (close == std::string_view::npos) throw Error("pattern: unclosed '('");
      const bool isFrac = (close == i + 2 && pat[i + 1] == 'f');
      toks.push_back({isFrac ? Tok::Frac : Tok::Cap, 0});
      i = close + 1;
      continue;
    }
    const int hi = hexVal(c);
    const int lo = (i + 1 < pat.size()) ? hexVal(pat[i + 1]) : -1;
    if (hi < 0 || lo < 0)
      throw Error("pattern: bad hex byte at offset " + std::to_string(i));
    toks.push_back({Tok::Lit, uint8_t((hi << 4) | lo)});
    i += 2;
  }
  return toks;
}

// Matches toks at data+pos. On success returns true and, if the pattern has a
// (v) token, writes the captured byte to *capture.
inline bool matchAt(const uint8_t* data, size_t size, size_t pos,
                    const std::vector<Tok>& toks, int* capture,
                    int* fraction = nullptr) {
  if (pos + toks.size() > size) return false;
  for (size_t i = 0; i < toks.size(); ++i) {
    const uint8_t b = data[pos + i];
    switch (toks[i].kind) {
      case Tok::Lit:
        if (b != toks[i].byte) return false;
        break;
      case Tok::Any:
        break;
      case Tok::Cap:
        if (capture) *capture = b;
        break;
      case Tok::Frac:
        if (fraction) *fraction = b;
        break;
    }
  }
  return true;
}

inline std::vector<size_t> findAll(const uint8_t* data, size_t size,
                                   const std::vector<Tok>& toks) {
  std::vector<size_t> hits;
  if (toks.empty() || size < toks.size()) return hits;
  for (size_t pos = 0; pos + toks.size() <= size; ++pos)
    if (matchAt(data, size, pos, toks, nullptr)) hits.push_back(pos);
  return hits;
}

// ===========================================================================
// parameter registry
// ===========================================================================

struct Form {
  std::string pattern;            // hex DSL; (v) captures the value byte
  std::optional<int> fixedValue;  // set when the pattern itself implies a value
  std::string note;
};

// A parameter is described by the pattern of its OWN entry, searched anywhere in
// the body. This is deliberate: parameters share the record region and each new
// one changes its neighbours' surroundings, so anchoring a parameter to a long
// stretch of context does not scale. Instead:
//
//   1. anomalies  — patterns we know we cannot decode  -> UnknownEncoding
//   2. present    — the parameter's entry is there     -> Ok, value decoded
//   3. neither    — the entry is absent                -> AssumedDefault
//
// Step 3 rests on a rule verified many times over: a parameter at its default
// (0 for levels and envelope stages) has NO entry in the file at all. It is
// reported as assumed rather than verified, and the CLI marks it with '*', so
// an unrecognised layout can never silently masquerade as a real reading.
struct ParamSpec {
  std::string id;
  std::string label;
  std::vector<Form> present;
  std::vector<Form> anomalies;
  int defaultValue = 0;
  std::string defaultNote;
  std::vector<std::pair<int, std::string>> valueLabels;
  bool firstMatch = false;  // pattern occurs more than once; the first wins
  int valueBias = 0;        // added to a CAPTURED value (signed params store
                            // display+64, so the bias is -64)
  bool fixedPoint = false;  // value is (capture + bias) + (f)/256
  bool pack16 = false;      // payload is 2 bytes big-endian: (v)*256 + (f)
  int postShift = 0;        // >> applied after unpacking
  int postMod = 0;          // % applied after the shift (extracts one field)
  int postDiv = 0;          // / applied after the shift (extracts the other)
};

// ---------------------------------------------------------------------------
// Add parameters here. One "present" form per encoding you have verified.
// ---------------------------------------------------------------------------
inline const std::vector<ParamSpec>& registry() {
  static const std::vector<ParamSpec> specs = {
      {
          "op_a_level", "Operator A level",
          {{"20 00 7F 00 (v) 08 00 00 19", std::nullopt,
            "00 <value> after 20 00 7F (verified: 42, 127)"}},
          {{"20 00 7F 46 00 00", std::nullopt, "op A level = 64 encodes as 46 00 00"},
           {"20 00 7F 00 12 00 00", std::nullopt,
            "op A level = 63 encodes as 00 12 00 00 (0x3F matched, not literal)"}},
          0, "no entry -> level 0", {},
      },
      {
          "op_b_level", "Operator B level",
          {{"20 00 7F 10 00 ?? 08 00 11 (v) 16 00 00 18", std::nullopt,
            "TENTATIVE, one sample (verified: 84 -> 0x54)"}},
          {{"20 00 7F 10 00 00 08 00 00 4E", std::nullopt, "op B level = 64 encodes as 4E"},
           {"20 00 7F 10 00 00 08 00 00 0A", std::nullopt, "op B level = 127 encodes as 0A"}},
          0, "no entry -> level 0", {},
      },
      {
          "op_a_attack", "Operator A attack",
          {{"00 30 00 51 (v) 00 20 00 7F", std::nullopt,
            "value stored LITERALLY (verified: 64, 78, 127)"}},
          {},
          0, "no entry -> attack 0", {},
      },
      {
          "op_b_attack", "Operator B attack",
          {{"20 00 7F 00 00 00 (v) 08 00 00 16", std::nullopt,
            "value stored LITERALLY (verified: 48 -> 0x30, 127 -> 0x7F)"}},
          {{"20 00 7F 4C 00 02", std::nullopt,
            "op B attack = 64 encodes as 0x4C (76), not 0x40"}},
          0, "no entry -> attack 0", {},
      },
      {
          // ALWAYS present, in every sample. The byte before the FIRST "00 7F"
          // in the body is the decay value: 32 in all 33 untouched files, and
          // 23 / 64 / 127 in the three AD files. No exceptions.
          "op_a_decay", "Operator A decay",
          {{"(v) 00 7F", std::nullopt,
            "always stored literally before the first 00 7F "
            "(verified: 23, 32, 64, 127)"}},
          {},
          32, "init default is 32, not 0", {},
          /*firstMatch=*/true,
      },
      {
          "op_a_end", "Operator A end level",
          {{"30 00 11 (v) 10 00 31", std::nullopt,
            "value stored LITERALLY (verified: 85 -> 0x55)"},
           {"00 0A 30 00 31 20 00 7F 18 00 11", 0,
            "end 0 has no value byte, only a structural change (verified: AE00)"}},
          {{"30 00 02 44 00 31", std::nullopt,
            "op A end = 64 encodes as 0x44 (68), not 0x40"}},
          127, "no entry -> end 127 (the init default)", {},
      },
      {
          // RETRACTED FORM: "10 00 02 08" was read as op B decay 0 on the
          // strength of BD000, the only sample with that value. H175-H177
          // carry the same three bytes and the device shows op B decay at its
          // default 32 in all three (checked on the hardware). So 0x02 does
          // not mean "op B decay 0", and the form is gone rather than
          // narrowed - a pattern that fits one sample and is contradicted by
          // three is not evidence.
          //
          // What 0x02 and 0x04 are is still open, and the op B DELAY samples
          // (H172-H174) narrowed it by killing two readings:
          //
          //   file        page-3 field   byte    op B decay
          //   ---------   ------------   ----    ----------
          //   init        11 + 1 byte    04      32
          //   H173 BDL64  11 + 1 byte    04      32
          //   H174 BDL14  51 + 5 bytes   04      32
          //   H172 BDL127 53 + 5 bytes   04      32
          //   H175-H177   31 + 3 bytes   02      32
          //   H213 BD000  11 + 1 byte    02       0
          //
          // NOT a page-3 group marker: H172/H174 change page 3 and keep 0x04.
          // NOT simply "the page-3 field got longer" either, or the 5-byte
          // files would flip too - only the 3-byte form does. What survives is
          // an exact but unexplained correlation: 0x02 appears when op A
          // delay is stored explicitly, and in BD000. A plain match length
          // could still do this if the copy is anchored differently in each
          // case, but nothing here shows that.
          "op_b_decay", "Operator B decay",
          {{"10 00 13 (v) 08", std::nullopt,
            "literal after 10 00 13 (verified: 32 in AD064, 64, 127)"},
           {"10 00 04 08", 32, "op B decay 32, absorbed into a match "
                               "(verified: init and most files)"}},
          // BD000 is now undecodable rather than wrong: its 0 is real (the
          // device was read) but nothing in the file can be attributed to it,
          // so reporting the 32 default here would be a silent wrong answer.
          {{"10 00 02 08 00 11", std::nullopt,
            "op B decay 0 is known to be set in BD000, but 0x02 cannot be "
            "attributed to it - H175-H177 have 0x02 with decay 32"}},
          32, "init default is 32", {},
      },
      {
          // OP A DELAY (page 3) - verified by H175/H176/H177, which differ
          // from each other in exactly ONE payload byte (plus name, save
          // counter and checksum):
          //
          //   delay  64   ... 10 00 02 08 00 31 40 00 01 48 ...
          //   delay  81   ... 10 00 02 08 00 31 51 00 01 48 ...
          //   delay 127   ... 10 00 02 08 00 31 7F 00 01 48 ...
          //
          // "08 00 31" occurs in exactly those three files and nowhere in the
          // other 82, so the anchor is unambiguous. The 0x31 is the same
          // length-prefixed field header seen elsewhere (2 payload bytes);
          // the second payload byte is 0x00 in all three samples. It is
          // probably a 1/256 fraction, as for harm and detune, but a whole
          // number is the only case sampled - so it is matched as a literal
          // 0x00 and NOT reported as a fraction.
          //
          // With the value absent the field collapses to a zero-length header:
          // "08 00 11 01" (59 of the 85 samples), which reads as 0 - the init
          // default. Files with neither form fall through to the default.
          //
          // Side note on why 127 behaves differently downstream: H175 and H176
          // are byte-identical after the value, but H177 diverges ~19 bytes
          // later and is one byte longer overall. 0x7F occurs elsewhere in the
          // record, so the compressor found a different match once the delay
          // byte became 0x7F. Same parameter, same encoding, different
          // downstream layout - the usual consequence of LZ matching.
          "op_a_delay", "Operator A delay",
          {{"08 00 31 (v) 00", std::nullopt,
            "literal, 2-byte field (verified: 64, 81, 127)"},
           {"08 00 11 01", 0, "zero-length field -> delay 0 (init default)"}},
          {},
          0, "init default is 0", {},
      },
      {
          // OP B DELAY (page 3) - H172/H173/H174. Its value sits FIVE bytes
          // into the page-3 field, not first as op A delay's does:
          //
          //   B delay  14   ... 08 00 51 01 00 01 00 0E 06 ...
          //   B delay 127   ... 08 00 53 01 00 01 00 7F 06 ...
          //   B delay  64   ... 08 00 11 01 18 00 00 06 ...   NOTHING LITERAL
          //
          // Compare the op A delay files, which use the same "08 00" marker:
          //
          //   A delay  64   ... 08 00 31 40 00 01 48 ...
          //
          // So "08 00" introduces one page-3 group holding both delays, and
          // the header byte after it grows with what the group carries: 0x11
          // with one payload byte when both delays are absorbed, 0x31 with
          // three when op A's value is there, 0x51/0x53 with five when op B's
          // is. High nibble = payload byte count fits all five observed
          // shapes. Treat that as a pattern that fits rather than a proven
          // rule: a framing test over the body showed that almost any byte
          // stream parses as (nibble length + payload), so the fit carries
          // little independent weight on its own.
          //
          // The header's low nibble differs between the two op B samples
          // (0x51 for 14, 0x53 for 127) and is NOT decoded, hence the "??".
          //
          // 64 is absorbed here as it is nearly everywhere else, so H173 has
          // no literal value at all - see the anomaly below.
          "op_b_delay", "Operator B delay",
          {{"08 00 ?? 01 00 01 00 (v)", std::nullopt,
            "literal, 5th payload byte (verified: 14, 127)"}},
          // H173 holds op B delay 64 (read off the device) and stores nothing
          // that can be attributed to it: the only thing separating it from an
          // init save in this region is one byte in the following token
          // ("00 00 06" against init's "00 02 06"), which is a match choice,
          // not a value. Reported as unknown rather than as the 0 default,
          // which would be a silent wrong answer.
          {{"11 01 18 00 00 06", std::nullopt,
            "op B delay 64 is set but fully absorbed; the only differing byte "
            "here is a match choice, not the value (H173)"}},
          0, "init default is 0", {},
      },
      {
          // PAGE 3 BOOLEANS - op A/B trig and reset, all "on" in init.
          // H168-H171, one boolean off per file, 14-character names.
          //
          // These are the first NON-NUMERIC parameters in the set, and the
          // first ones the pattern method cannot really decode. Each file
          // produces a distinct page-3 group, so the booleans do live inside
          // the "08 00" group - but every byte that differs is entangled with
          // the match tokens around it, and with one sample per boolean there
          // is no way to isolate the bit. Lined up from "08 00":
          //
          //   all on (H173)  11 01 18 00 00 06 00 11        | 7F 60 ...
          //   op A trg off   00 18 00 00 04 00 31 01 00     | 7F 60 ...
          //   op B trg off   13 01 48 00 31 01 00           | 7F 60 ...
          //   op B rst off   11 01 48 00 00 04 00 11        | 7F 60 ...
          //   op A rst off   02 46 00 51 01 00 01 00        | 7F 60 ...
          //
          // op B rst off differs from all-on in exactly two bytes (18 -> 48
          // and 06 -> 04) with no length change - and both of those bytes are
          // known to move for unrelated reasons elsewhere, so neither can be
          // called the bit.
          //
          // PROVISIONAL, and deliberately so: each form below is the whole
          // group, matched as one string, and rests on ONE file. That is
          // exactly the shape of evidence that produced the retracted
          // "10 00 02 08 = op B decay 0" reading. They are registered because
          // a whole-group string is at least unique across all 89 samples
          // (checked), but a second sample of any of these - or any file with
          // two page-3 edits at once - is expected to break them.
          "op_a_trg", "Operator A trig",
          {{"08 00 00 18 00 00 04 00 31 01 00", 0,
            "whole-group match, ONE sample (H171) - provisional"}},
          {},
          1, "init default is on", {{0, "off"}, {1, "on"}},
      },
      {
          "op_a_rst", "Operator A reset",
          {{"08 00 02 46 00 51 01 00 01 00", 0,
            "whole-group match, ONE sample (H168) - provisional"}},
          {},
          1, "init default is on", {{0, "off"}, {1, "on"}},
      },
      {
          "op_b_trg", "Operator B trig",
          {{"08 00 13 01 48 00 31 01 00", 0,
            "whole-group match, ONE sample (H170) - provisional"}},
          {},
          1, "init default is on", {{0, "off"}, {1, "on"}},
      },
      {
          "op_b_rst", "Operator B reset",
          {{"08 00 11 01 48 00 00 04 00 11", 0,
            "whole-group match, ONE sample (H169) - provisional"}},
          {},
          1, "init default is on", {{0, "off"}, {1, "on"}},
      },
      {
          // PHASE RESET (page 3) - and the answer to the H213 mystery.
          //
          // It does NOT live in the "08 00" page-3 group; all four phrt files
          // are byte-identical there. It sits just before op A decay, in the
          // slot right after the detune/feedback anchor "78 3F":
          //
          //   all (H167)     78 3F 22 00 00 3E 00 31 20 00 7F
          //   value 1        78 3F 22 00 11 01 3E 00 31 20 00 7F
          //   C     (H166)   78 3F 22 00 11 02 3E ...
          //   A+B   (H165)   78 3F 22 00 11 03 3E ...
          //   A+B2  (H164)   78 3F 1F 00 11 04 3E ...
          //
          // So the field is "11 <value>" with 0x3E after it, and "all" takes
          // the zero-payload form. Across all 93 samples the partition is
          // exact: 49 files carry no field at all, 40 carry value 1, and one
          // each carries 2, 3 and 4. Nothing matches twice.
          //
          // THIS IS THE H213 BREAK. The 49 fieldless files are every save
          // BEFORE H213; the 40 files carrying value 1 are every save from
          // H213 (BD000) onward. What was earlier recorded as an unexplained
          // "11 01 3E" era marker was simply phase reset holding value 1: it
          // was changed at the BD000 save and left there for 45 consecutive
          // saves, which is why it tracked save order and no parameter. H167
          // set it back to "all" and the field went away again.
          //
          // Two consequences worth keeping in mind:
          //   - the ADL, BDL and boolean files all carry phrt = off, so they
          //     were never quite init patches;
          //   - the earlier localisation was right for the right reason - LZ
          //     matching propagates forward, the divergence sat immediately
          //     before op A decay, and that is exactly where this field is.
          //
          // The enum, with value 1 named from the device:
          //
          //   0  all     default; stored as the zero-payload form
          //   1  off     what the patch sat on from H213 to H168
          //   2  C
          //   3  A+B
          //   4  A+B2
          //
          // A contiguous index from zero, i.e. a plain C enum cast to an int -
          // which is what the byte values say, four of them measured directly
          // and consecutive. Whether a fifth option exists above A+B2 is not
          // known; nothing in the samples bounds the range.
          //
          // ONE INFERENCE, flagged: 0 = all is not measured. H167 (all) stores
          // the ZERO-PAYLOAD form, so the byte is absent, not literally 0x00.
          // Reading "no payload" as 0 matches how op A delay behaves and is
          // the natural fit for an enum starting at zero, but a file with
          // phrt = all and the field emitted explicitly would be needed to see
          // the literal - and 64-style absorption suggests such a file may
          // simply not exist.
          //
          // So the H213 story reads: phase reset was switched OFF at the BD000
          // save and left off for 45 consecutive saves, through every ratio,
          // envelope, mix, harm, detune, feedback, delay and boolean sample.
          "phase_reset", "Phase reset (phrt)",
          {{"11 (v) 3E", std::nullopt,
            "1-byte field before 0x3E (verified: 1 = off, 2 = C, 3 = A+B, "
            "4 = A+B2)"},
           {"78 3F ?? 00 00 3E", 0,
            "zero-payload form, read as 0 = all (verified: H167)"}},
          {},
          0, "init default is all; absent in every save before H213",
          {{0, "all"}, {1, "off"}, {2, "C"}, {3, "A+B"}, {4, "A+B2"}},
      },
      {
          "op_b_end", "Operator B end level",
          {{"10 00 31 20 00 (v) 08 00 11 01 48", std::nullopt,
            "literal (verified: 46 -> 0x2E)"}},
          {{"10 00 11 20 0E 00 00 16", std::nullopt,
            "op B end = 64 absorbed into a match"},
           {"10 00 11 20 06 00 00 16", std::nullopt,
            "op B end = 0, no literal zero"}},
          127, "init default is 127, as for op A end", {},
      },
      {
          // OP B1 / B2 RATIOS - the pair is PACKED INTO ONE NUMBER.
          //
          //   packed >> 1  ==  19 * b2 + b1      (both use op C's 19-entry
          //                                       coarse table)
          //   bit 0 of packed is a separate flag, set in 2 of 8 samples and
          //   not yet identified.
          //
          //   pair          packed   >>1   b1  b2
          //   0.25 / 0.25   0x0000     0    0   0
          //   1.00 / 1.00   0x0078    60    3   3   (init)
          //   2.00 / 0.50   0x002F    23    4   1
          //  16.00 / 0.75   0x0070    56   18   2
          //  11.00 / 1.00   0x008C   140>>1=70 13  3
          //   0.50 / 6.00   0x0132   153    1   8
          //  16.00 / 13.00  0x025F   303   18  15
          //  16.00 / 16.00  0x02D0   360   18  18
          //
          // The payload is a variable-length BIG-ENDIAN integer with a length
          // prefix: 0x13 = no bytes (value 0), 0x23 = one byte, 0x33 = two.
          // Payload bytes here are 8-bit (0x8C and 0xD0 both occur), unlike
          // the 7-bit values elsewhere.
          "op_b1_ratio", "Operator B1 ratio",
          {{"38 00 33 (v) (f) 3F", std::nullopt,
            "2-byte packed pair (verified: 0.50/6.00, 16/13, 16/16)"},
           {"38 00 23 (v) 3F", std::nullopt,
            "1-byte packed pair (verified: init 1/1, 2.00/0.50, 16/0.75, "
            "11/1.00)"},
           {"38 00 13 3F", 0, "packed pair 0 = both 0.25 (verified: BR2525)"}},
          {},
          3, "init default 1.00 = index 3",
          {
              {0, "0.25"}, {1, "0.50"}, {2, "0.75"}, {3, "1.00"}, {4, "2.00"},
              {5, "3.00 (inferred)"}, {6, "4.00 (inferred)"}, {7, "5.00 (inferred)"},
              {8, "6.00"}, {9, "7.00 (inferred)"}, {10, "8.00 (inferred)"},
              {11, "9.00 (inferred)"}, {12, "10.00 (inferred)"}, {13, "11.00"},
              {14, "12.00 (inferred)"}, {15, "13.00"}, {16, "14.00 (inferred)"},
              {17, "15.00 (inferred)"}, {18, "16.00"}
          },
          false, 0, false, true, 1, 19, 0,
      },
      {
          "op_b2_ratio", "Operator B2 ratio",
          {{"38 00 33 (v) (f) 3F", std::nullopt,
            "2-byte packed pair (verified: 0.50/6.00, 16/13, 16/16)"},
           {"38 00 23 (v) 3F", std::nullopt,
            "1-byte packed pair (verified: init 1/1, 2.00/0.50, 16/0.75, "
            "11/1.00)"},
           {"38 00 13 3F", 0, "packed pair 0 = both 0.25 (verified: BR2525)"}},
          {},
          3, "init default 1.00 = index 3",
          {
              {0, "0.25"}, {1, "0.50"}, {2, "0.75"}, {3, "1.00"}, {4, "2.00"},
              {5, "3.00 (inferred)"}, {6, "4.00 (inferred)"}, {7, "5.00 (inferred)"},
              {8, "6.00"}, {9, "7.00 (inferred)"}, {10, "8.00 (inferred)"},
              {11, "9.00 (inferred)"}, {12, "10.00 (inferred)"}, {13, "11.00"},
              {14, "12.00 (inferred)"}, {15, "13.00"}, {16, "14.00 (inferred)"},
              {17, "15.00 (inferred)"}, {18, "16.00"}
          },
          false, 0, false, true, 1, 0, 19,
      },
      {
          // FEEDBACK: plain 0..127, literal after "78 3F 00 00 00".
          //   127 -> 78 3F 00 00 00 7F 30      35 -> 78 3F 00 00 00 23 30
          // 64 is absorbed into a match yet again (78 3F 3C 00 00 30 00 00 06),
          // which is now the sixth parameter where exactly 64 disappears.
          "feedback", "Feedback",
          {{"78 3F 00 00 00 (v) 30", std::nullopt,
            "literal (verified: 35, 127)"}},
          {{"78 3F 3C 00 00 30 00 00 06", std::nullopt,
            "feedback = 64 absorbed into a match"}},
          0, "init default 0", {},
      },
      {
          // DETUNE: fixed point like harm, but UNSIGNED - bias 0, not 63.
          //
          //   127.00   78 3F 00 7F        127 + 0/256    = 127.000
          //    64.00   78 3F 00 40        64  + 0/256    =  64.000
          //    94.24   78 3F 00 5E 3F 25  94  + 63/256   =  94.246 -> 94.24
          //
          // Same 1/256 fraction and the same truncating display as harm, so
          // the fixed-point FORM is shared, but the zero point is not: harm
          // biases by 63, detune by 0. Each such parameter needs its own bias
          // pinned by a sample.
          "detune", "Detune",
          {{"78 3F 00 (v) (f) 25", std::nullopt,
            "fixed point, fraction present (verified: 94.24)"},
           {"78 3F 00 (v) ?? 00 11", std::nullopt,
            "fixed point, whole number (verified: 64.00, 127.00)"}},
          {},
          0, "init default 0.00", {}, false, 0, true,
      },
      {
          // HARM / osc shape: a signed FIXED-POINT parameter, not an integer.
          // Two bytes: (integer + 63) then a 1/256 fraction. The fraction byte
          // is a plain zero for whole numbers, so it is absorbed and only one
          // byte survives - which is why whole and fractional values look like
          // different encodings.
          //
          //   0.00   3F      (63-63) + 0/256   =   0.000
          //  +26.00  59      (89-63) + 0/256   = +26.000
          //  -26.00  25      (37-63) + 0/256   = -26.000
          //   +7.16  46 2B   (70-63) + 43/256  =  +7.168  -> shown as +7.16
          //   -4.70  3A 4B   (58-63) + 75/256  =  -4.707  -> shown as -4.70
          //
          // The display TRUNCATES toward zero rather than rounding; both
          // fractional samples confirm it. The byte before "78" flags the
          // form: 0x23 whole, 0x32 fractional.
          "harm", "Harm (osc shape)",
          {{"38 00 32 78 (v) (f)", std::nullopt,
            "fixed point, fraction present (verified: +7.16, -4.70)"},
           {"38 00 23 78 (v)", std::nullopt,
            "fixed point, whole number (verified: 0.00, +26.00, -26.00)"}},
          {},
          0, "init default 0.00", {}, false, -63, true,
      },
      {
          // The first SIGNED parameter. Range -64..+63, stored as value + 64,
          // so the default -64 stores as 0 and is never a literal - which is
          // exactly why it is invisible in init, like every other zero.
          //
          //   mix -20 -> 0x2C (44)   mix 0 -> 0x40 (64)   mix +63 -> 0x7F (127)
          //
          // All three sit at the same body offset in their files, in the slot
          // "?? 00 31 <stored> 00 01 3E 00 31".
          "mix", "Mix (carriers X/Y)",
          {{"00 31 (v) 00 01 3E 00 31", std::nullopt,
            "stored as mix + 64 (verified: -20, 0, +63)"}},
          {},
          -64, "init default -64, stored as 0 and absorbed", {}, false, -64,
      },
      {
          // OP A HAS THE FINER TABLE - 36 entries: 0.25 steps to 5, then 0.5
          // to 10, then whole numbers to 16. All six measurements fit exactly.
          // Confirmed with the device: op C has fewer options than op A. (The
          // two lists were once reported the other way round; the deciding
          // evidence is that 1.25 and 2.75 exist ONLY in the fine table and
          // appear in the op A samples.)
          "op_a_ratio", "Operator A ratio",
          {{"73 03 00 (v) 00 00", std::nullopt,
            "73 03 00 <index> (verified: 0.75, 1.25, 2.75, 8.00, 16.00)"},
           {"28 00 00 36 00 33 00", 0,
            "index 0 = 0.25, absorbed (verified: AR025)"}},
          {},
          3, "init default 1.00 = index 3",
          {
              {0, "0.25"}, {1, "0.50 (inferred)"}, {2, "0.75"},
              {3, "1.00"}, {4, "1.25"}, {5, "1.50 (inferred)"},
              {6, "1.75 (inferred)"}, {7, "2.00 (inferred)"}, {8, "2.25 (inferred)"},
              {9, "2.50 (inferred)"}, {10, "2.75"}, {11, "3.00 (inferred)"},
              {12, "3.25 (inferred)"}, {13, "3.50 (inferred)"},
              {14, "3.75 (inferred)"}, {15, "4.00 (inferred)"},
              {16, "4.25 (inferred)"}, {17, "4.50 (inferred)"},
              {18, "4.75 (inferred)"}, {19, "5.00 (inferred)"},
              {20, "5.50 (inferred)"}, {21, "6.00 (inferred)"},
              {22, "6.50 (inferred)"}, {23, "7.00 (inferred)"},
              {24, "7.50 (inferred)"}, {25, "8.00"}, {26, "8.50 (inferred)"},
              {27, "9.00 (inferred)"}, {28, "9.50 (inferred)"},
              {29, "10.00 (inferred)"}, {30, "11.00 (inferred)"},
              {31, "12.00 (inferred)"}, {32, "13.00 (inferred)"},
              {33, "14.00 (inferred)"}, {34, "15.00 (inferred)"}, {35, "16.00"}
          },
      },
      {
          // OP C HAS THE COARSER TABLE - 19 entries: 0.25 / 0.5 / 0.75 / 1,
          // then whole numbers to 16. Confirmed with the device.
          "op_c_ratio", "Operator C ratio",
          {{"10 (v) 38 00 23", std::nullopt,
            "10 <ratio index> (verified: 0.75, 2.00, 9.00, 16.00)"},
           {"28 00 53 03 00 00", 0, "ratio index 0 = 0.25 (verified: CR025)"}},
          {{"28 00 00 3A 00 33 00", std::nullopt,
            "ratio 0.50 = index 1; 0x01 is the match opcode so it is never "
            "written as a literal"}},
          3, "no entry -> index 3 = ratio 1.00",
          {
              {0, "0.25"}, {1, "0.50 (inferred)"}, {2, "0.75"}, {3, "1.00"},
              {4, "2.00"}, {5, "3.00 (inferred)"}, {6, "4.00 (inferred)"},
              {7, "5.00 (inferred)"}, {8, "6.00 (inferred)"}, {9, "7.00 (inferred)"},
              {10, "8.00 (inferred)"}, {11, "9.00"}, {12, "10.00 (inferred)"},
              {13, "11.00 (inferred)"}, {14, "12.00 (inferred)"},
              {15, "13.00 (inferred)"}, {16, "14.00 (inferred)"},
              {17, "15.00 (inferred)"}, {18, "16.00"}
          },
      },
  };
  return specs;
}

enum class Status {
  Ok,              // an entry was found and decoded
  AssumedDefault,  // no entry present, so the documented default is reported
  Ambiguous,       // more than one entry matched
  UnknownEncoding, // a known-undecodable pattern, or nothing usable
};

inline const char* statusName(Status s) {
  switch (s) {
    case Status::Ok: return "ok";
    case Status::AssumedDefault: return "assumed-default";
    case Status::Ambiguous: return "ambiguous";
    case Status::UnknownEncoding: return "unknown-encoding";
  }
  return "?";
}

struct ParamResult {
  std::string id;
  std::string label;
  Status status = Status::UnknownEncoding;
  int value = -1;
  std::string formNote;
  std::string valueLabel;
  size_t entryBodyOffset = 0;
  size_t entryFileOffset = 0;
  Bytes entryBytes;  // populated for UnknownEncoding, for inspection
  double real = 0.0;      // fixed-point parameters: the true value
  bool hasReal = false;
  bool signedValue = false;  // show an explicit + sign

  bool ok() const { return status == Status::Ok; }
  bool usable() const {
    return status == Status::Ok || status == Status::AssumedDefault;
  }
};

// A record dense enough to be laid out literally carries "28 00 F1 16" where a
// sparse one carries "28 00 <alg-ish>". In such a file the patterns below are
// invalid: they were all derived from records where a parameter's neighbours
// were matched away, and matching them against a laid-out record produces
// plausible-looking nonsense. The field offsets for this case ARE known (see
// THE RECORD LAYOUT at the top of this file, 19 fields confirmed twice by
// H162/H163), but no reader has been written for them yet - so report unknown
// rather than a wrong number.
inline bool denseRecord(const Patch& patch) {
  static const std::vector<Tok> marker = parsePattern("28 00 F1 16");
  return !findAll(patch.body(), patch.bodySize(), marker).empty();
}

inline ParamResult decodeParam(const Patch& patch, const ParamSpec& spec) {
  ParamResult r;
  r.id = spec.id;
  r.label = spec.label;

  if (denseRecord(patch)) {
    r.status = Status::UnknownEncoding;
    r.formNote = "record is laid out literally (28 00 F1 16); the sparse-file "
                 "patterns do not apply - see THE RECORD LAYOUT in dn2pst.hpp";
    return r;
  }

  // 1. known-undecodable patterns win, so a bad reading is never reported.
  for (const Form& f : spec.anomalies) {
    const std::vector<Tok> toks = parsePattern(f.pattern);
    const std::vector<size_t> hits = findAll(patch.body(), patch.bodySize(), toks);
    if (hits.empty()) continue;
    r.status = Status::UnknownEncoding;
    r.formNote = f.note;
    r.entryBodyOffset = hits[0];
    r.entryFileOffset = patch.bodyOffset + hits[0];
    const size_t avail = patch.bodySize() - hits[0];
    r.entryBytes.assign(patch.body() + hits[0],
                        patch.body() + hits[0] + (avail < 14 ? avail : 14));
    return r;
  }

  // 2. the parameter's own entry. Forms are tried IN ORDER and the first one
  // that matches wins, so a longer, more specific form can be listed ahead of
  // a shorter one that is a prefix of it.
  std::vector<ParamResult> found;
  for (const Form& f : spec.present) {
    found.clear();
    const std::vector<Tok> toks = parsePattern(f.pattern);
    bool hasFrac = false;
    for (const Tok& t : toks) if (t.kind == Tok::Frac) hasFrac = true;
    for (size_t hit : findAll(patch.body(), patch.bodySize(), toks)) {
      int cap = -1, frac = 0;
      matchAt(patch.body(), patch.bodySize(), hit, toks, &cap, &frac);
      int value;
      if (f.fixedValue) {
        value = *f.fixedValue;
      } else if (cap >= 0) {
        int raw = (spec.pack16 && hasFrac) ? (cap * 256 + frac) : cap;
        raw += spec.valueBias;
        raw >>= spec.postShift;
        if (spec.postMod) raw %= spec.postMod;
        else if (spec.postDiv) raw /= spec.postDiv;
        value = raw;
      } else {
        continue;
      }
      ParamResult d = r;
      d.status = Status::Ok;
      d.value = value;
      if (spec.fixedPoint && cap >= 0) {
        d.real = double(cap + spec.valueBias) + double(frac) / 256.0;
        d.hasReal = true;
      }
      d.formNote = f.note;
      d.entryBodyOffset = hit;
      d.entryFileOffset = patch.bodyOffset + hit;
      found.push_back(d);
    }
    if (!found.empty()) break;
  }

  if (found.size() == 1) {
    r = found[0];
  } else if (found.size() > 1 && spec.firstMatch) {
    r = found[0];
  } else if (found.size() > 1) {
    r.status = Status::Ambiguous;
    return r;
  } else {
    // 3. absent -> documented default.
    r.status = Status::AssumedDefault;
    r.value = spec.defaultValue;
    r.formNote = spec.defaultNote;
    if (spec.fixedPoint) { r.real = spec.defaultValue; r.hasReal = true; }
  }
  r.signedValue = spec.valueBias < 0;
  for (const auto& vl : spec.valueLabels)
    if (vl.first == r.value) { r.valueLabel = vl.second; break; }
  return r;
}

inline std::vector<ParamResult> decodeAll(const Patch& patch) {
  std::vector<ParamResult> out;
  out.reserve(registry().size());
  for (const ParamSpec& s : registry()) out.push_back(decodeParam(patch, s));
  return out;
}

inline std::optional<ParamResult> decodeParamById(const Patch& patch,
                                                  std::string_view id) {
  for (const ParamSpec& s : registry())
    if (s.id == id) return decodeParam(patch, s);
  return std::nullopt;
}

// ===========================================================================
// raw fields: located but not decoded
// ===========================================================================
//
// Some things are easy to LOCATE but not yet decoded — the algorithm is the
// current example. A FieldSpec pins the bytes down and reports them raw, with
// a lookup of the byte patterns whose meaning is known. That is honest about
// what we know and still gives the CLI something to print.
//
// Location rule: the END anchor must occur exactly once in the body; the field
// is the bytes between the LAST occurrence of the start anchor before it and
// that end anchor. ("28 00" alone appears twice in some files, so searching
// backwards from the unique end anchor is what makes this reliable.)

struct KnownField {
  std::string bytes;  // hex pattern, same DSL (no captures)
  std::string meaning;
};

struct FieldSpec {
  std::string id;
  std::string label;
  std::string startAnchor;  // field begins right after this
  std::string endAnchor;    // field ends right before this; must be unique
  std::vector<KnownField> known;
};

// ---------------------------------------------------------------------------
// Add raw fields here.
// ---------------------------------------------------------------------------
inline const std::vector<FieldSpec>& fields() {
  static const std::vector<FieldSpec> specs = {
      {
          "alg_ratio_slot",
          "Algorithm + op C ratio slot",
          "02 00 ?? 28 00",
          "78 3F",
          {
              {"01 38 00 23", "alg 1, ratio 1.00 (defaults)"},
              {"03 38 00 23", "alg 2, ratio 1.00"},
              {"00 36 00 53 03 00 00", "alg 4, ratio 1.00"},
              {"12 07 38 00 23", "alg 8, ratio 1.00"},
              {"10 0B 38 00 23", "alg 1, ratio 9.00"},
              {"10 12 38 00 23", "alg 1, ratio 16.00"},
              {"53 03 00 00", "alg 1, ratio 0.25"},
          },
      },
  };
  return specs;
}

struct FieldResult {
  std::string id;
  std::string label;
  Status status = Status::UnknownEncoding;
  Bytes bytes;
  std::string meaning;  // empty when the pattern is not in the known list
  size_t bodyOffset = 0;
  size_t fileOffset = 0;

  bool ok() const { return status == Status::Ok; }
};

inline FieldResult extractField(const Patch& p, const FieldSpec& spec) {
  FieldResult r;
  r.id = spec.id;
  r.label = spec.label;

  const std::vector<Tok> endToks = parsePattern(spec.endAnchor);
  const std::vector<size_t> endHits =
      findAll(p.body(), p.bodySize(), endToks);
  if (endHits.empty()) { r.status = Status::UnknownEncoding; return r; }
  if (endHits.size() > 1) { r.status = Status::Ambiguous; return r; }

  const std::vector<Tok> startToks = parsePattern(spec.startAnchor);
  const std::vector<size_t> startHits =
      findAll(p.body(), p.bodySize(), startToks);
  size_t start = std::string::npos;
  for (size_t hit : startHits)
    if (hit + startToks.size() <= endHits[0]) start = hit + startToks.size();
  if (start == std::string::npos) { r.status = Status::UnknownEncoding; return r; }

  r.status = Status::Ok;
  r.bodyOffset = start;
  r.fileOffset = p.bodyOffset + start;
  r.bytes.assign(p.body() + start, p.body() + endHits[0]);

  for (const KnownField& k : spec.known) {
    const std::vector<Tok> kt = parsePattern(k.bytes);
    if (kt.size() != r.bytes.size()) continue;
    if (matchAt(r.bytes.data(), r.bytes.size(), 0, kt, nullptr)) {
      r.meaning = k.meaning;
      break;
    }
  }
  return r;
}

inline std::vector<FieldResult> extractAllFields(const Patch& p) {
  std::vector<FieldResult> out;
  out.reserve(fields().size());
  for (const FieldSpec& s : fields()) out.push_back(extractField(p, s));
  return out;
}

// ===========================================================================
// 8-bit / 7-bit block map
// ===========================================================================
//
// The body follows the MIDI convention: data bytes are 7-bit (<= 0x7F) and
// single bytes with the high bit set act as markers between blocks. Verified
// across 7 samples: the marker sequence is always
//
//     B1  B0  AC  F3  FF FF  ...  BA CE F0 ...
//
// and only the FIRST 7-bit block (the one after B1) changes size — it holds
// the parameter records. The blocks after B0 are fixed at 14 / 19 / 14 bytes.
// Because no byte in a 7-bit block has the high bit set, that block cannot
// contain continuation-bit varints: every token there is a single 0..127 value.

struct Block {
  size_t offset = 0;  // body-relative
  size_t size = 0;
  bool isMarker = false;  // true => a single byte >= 0x80
  uint8_t marker = 0;
};

// Size of the fixed trailer: <u32 checksum> 00 00 00 <seq+8> AA A1 DA AA.
// It is excluded from the map — the checksum is high-entropy, not 7-bit data.
inline constexpr size_t kTrailerSize = 12;

inline std::vector<Block> blockMap(const Patch& p) {
  std::vector<Block> out;
  const uint8_t* b = p.body();
  const size_t n =
      p.bodySize() > kTrailerSize ? p.bodySize() - kTrailerSize : p.bodySize();
  size_t start = 0;
  for (size_t i = 0; i < n; ++i) {
    if (b[i] < 0x80) continue;
    if (i > start) out.push_back({start, i - start, false, 0});
    out.push_back({i, 1, true, b[i]});
    start = i + 1;
  }
  if (start < n) out.push_back({start, n - start, false, 0});
  return out;
}

// The variable-length 7-bit block that carries the parameter records (the
// largest one; it is the block immediately after the first marker).
inline std::optional<Block> recordBlock(const Patch& p) {
  std::optional<Block> best;
  for (const Block& blk : blockMap(p))
    if (!blk.isMarker && (!best || blk.size > best->size)) best = blk;
  return best;
}

// ===========================================================================
// EXPERIMENTAL: LZ decompression
// ===========================================================================
//
// The payload is LZ77-compressed, and the patch NAME is inside the compressed
// stream. Proof: "FM INIT 00000" does not appear literally in its own file —
// it is stored as the 9 literals "FM INIT 0" followed by 01 00 10, while every
// name without a repeated character does appear literally.
//
// Match token, from six engineered names (init patch, name is the only variable):
//
//     name              compressed after the literals      dist  len   result
//     FM INIT XYZQW     (none - fully literal)              -     -    no repeat
//     FM INIT AAAAB     (none - fully literal)              -     -    run of 3
//     FM INIT ABCABC    (none - fully literal)              -     -    len 3 @ d3
//     FM INIT AAAAA     "FM INIT A"  + 01 00 10             1     4    AAAA
//     FM INIT 00000     "FM INIT 0"  + 01 00 10             1     4    0000
//     FM INIT AAAAAAA   "FM INIT A"  + 01 00 31             1     6    AAAAAA
//     FM INIT ABABAB    "FM INIT AB" + 02 00 51             2     4    ABAB
//
// TWO THINGS ARE NOW SETTLED.
//
// 1. MINIMUM MATCH LENGTH IS 4. AAAAB (a run of 3 after the first A) and ABCABC
//    (length 3 at distance 3) both stay fully literal, while AAAAA (a run of 4)
//    compresses. That is exactly what len = (v & 0x0F) + 4 predicts, and it was
//    predicted before these files existed.
//
// 2. THE FIRST TOKEN BYTE IS THE DISTANCE, NOT AN OPCODE.
//       distance 1 -> 01        distance 2 -> 02
//    This dissolves the "is this 0x01 a literal or an opcode" problem that has
//    blocked the decompressor: 0x01 was never an opcode. Every match seen until
//    now happened to be a repeated character, i.e. distance 1, which made the
//    distance byte look like a fixed opcode. A literal 0x01 (op A attack = 1)
//    and a distance-1 match legitimately start with the same byte, and the
//    decoder needs the surrounding grammar - not the byte - to tell them apart.
//
// THE TOKEN DOES NOT CARRY THE LENGTH. Seven known matches:
//
//     dist  len   token       hdr @ 0x2F   name
//      1     4    01 00 10        C0       FM INIT AAAAA   (13 ch)
//      1     5    01 00 51        C1       FM INIT AAAAAA  (14)
//      1     6    01 00 31        C3       FM INIT AAAAAAA (15)
//      1    14    01 00 41        4A       AAAAAAAAAAAAAAA (15)
//      2     4    02 00 51        D0       FM INIT ABABAB  (14)
//      2     5    02 00 41        D1       FM INIT ABABABA (15)
//      2    13    02 00 41        59       ABABABABABABABA (15)
//
// The last two are decisive: "02 00 41" encodes BOTH a length-5 and a
// length-13 match. Their compressed tails are byte-identical from the token
// onward; the files differ only in how many literals precede it (10 vs 2).
// So the three token bytes cannot contain the length, and every model built on
// "byte 3 encodes length" - including the 2^(len-4)-1 nibble idea - is dead.
//
// Where the length actually comes from is unresolved. The only thing that
// differs between those two files is the header at 0x2F (D1 vs 59), and that
// field is not a plain size either: read as LEB128 it gives 64/65/67/74/80/81/89
// with no consistent relationship to payload size, name length, or literal
// count. Note the third byte does track name length loosely - both 14-char
// names give 0x51 - but three of the four 15-char names give 0x41 and the
// fourth gives 0x31, so that is not it either.
//
// PRACTICAL CONCLUSION: engineered names have given all they can. Seven
// data points contradict every simple layout. The name match may even be a
// special "fill the name field" case, since the decoder knows the name length
// independently from the manifest. Cracking this needs known plaintext in the
// BODY, which is what the parameter samples provide - so the parameter work is
// both the useful path and the better route back to the compressor.
//
// This also explains the long-standing puzzle. A value is stored literally only
// when the compressor did not find an earlier match for it: op A level 42 and
// 65 are literals because 0x2A and 0x41 had not appeared yet, while 63 (0x3F,
// already present in "78 3F") and 64 (0x40, already present earlier in the
// body) were emitted as back-references instead. Same parameter, different
// encoding, purely because of what preceded it in the stream.

struct Decompressed {
  Bytes data;
  bool complete = false;   // reached the end without an unreadable token
  size_t tokensRead = 0;
  size_t literalFallbacks = 0;  // 0x01 bytes treated as literals; >0 means the
                                // opcode rule is still not fully understood
};

inline Decompressed decompress(const Patch& p) {
  Decompressed r;
  const Bytes& d = p.raw;
  const size_t start = p.nameOffset;
  const size_t end = d.size() > kTrailerSize ? d.size() - kTrailerSize : d.size();

  size_t i = start;
  while (i < end) {
    if (d[i] == 0x01 && i + 2 < end) {
      const unsigned v = (unsigned(d[i + 1]) << 7) | d[i + 2];
      const size_t dist = v >> 4;
      const size_t len = (v & 0x0F) + 4;
      if (dist > 0 && dist <= r.data.size()) {
        size_t s = r.data.size() - dist;
        for (size_t k = 0; k < len; ++k) r.data.push_back(r.data[s++]);
        i += 3;
        ++r.tokensRead;
        continue;
      }
      ++r.literalFallbacks;
    }
    r.data.push_back(d[i++]);
  }
  r.complete = true;
  return r;
}

// ===========================================================================
// payload parsing
// ===========================================================================

// expectedName: pass the manifest "Payload" value when available (that is the
// authoritative name); otherwise the name is located after the section marker.
inline Patch parsePayload(Bytes bytes,
                          const std::string* expectedName = nullptr) {
  static const uint8_t kMarker[4] = {0xBE, 0xEF, 0xBA, 0xCE};
  static const uint8_t kTailMagic[4] = {0xAA, 0xA1, 0xDA, 0xAA};

  Patch p;
  p.raw = std::move(bytes);
  if (p.raw.size() < 0x40) throw Error("payload too small");

  Header& h = p.header;
  std::memcpy(h.magic.data(), p.raw.data(), 4);
  h.magicOk = (h.magic[0] == 0xAC && h.magic[1] == 0x11 && h.magic[2] == 0xD3 &&
               h.magic[3] == 0x03);
  for (size_t i = 0x09; i < 0x0E && p.raw[i]; ++i) h.tag += char(p.raw[i]);
  h.saveCounter = p.raw[0x18];
  h.seq = p.raw[0x22];

  // section marker
  size_t marker = std::string::npos;
  for (size_t i = 0; i + 4 <= p.raw.size(); ++i) {
    if (std::memcmp(p.raw.data() + i, kMarker, 4) == 0) {
      marker = i;
      break;
    }
  }
  if (marker == std::string::npos) throw Error("payload: BE EF BA CE not found");
  h.markerOffset = marker;

  // name
  size_t nameAt = std::string::npos;
  if (expectedName && !expectedName->empty()) {
    const size_t at = std::string_view(
                          reinterpret_cast<const char*>(p.raw.data()),
                          p.raw.size())
                          .find(*expectedName, marker);
    if (at != std::string_view::npos &&
        at + expectedName->size() < p.raw.size() &&
        p.raw[at + expectedName->size()] == 0x00) {
      nameAt = at;
    }
  }
  if (nameAt == std::string::npos) {
    // first printable ASCII run after the marker's 09 00 <varint> 00 00 00
    for (size_t i = marker + 4; i < p.raw.size(); ++i) {
      if (p.raw[i] >= 0x20 && p.raw[i] < 0x7F) {
        nameAt = i;
        break;
      }
    }
    if (nameAt == std::string::npos) throw Error("payload: name not found");
  }

  size_t nul = nameAt;
  while (nul < p.raw.size() && p.raw[nul] != 0x00) ++nul;
  if (nul >= p.raw.size()) throw Error("payload: unterminated name");

  p.nameOffset = nameAt;
  p.name.assign(reinterpret_cast<const char*>(p.raw.data() + nameAt),
                nul - nameAt);
  p.bodyOffset = nul + 1;

  h.nameLenField.assign(p.raw.begin() + marker + 6, p.raw.begin() + nameAt);

  // trailer: <u32 checksum> 00 00 00 <seq+8> AA A1 DA AA
  Trailer& t = p.trailer;
  if (p.raw.size() >= 12) {
    const size_t end = p.raw.size();
    t.magicOk = std::memcmp(p.raw.data() + end - 4, kTailMagic, 4) == 0;
    t.seqEcho = p.raw[end - 5];
    t.checksum = rdU32(p.raw.data() + end - 12);
    t.seqEchoOk = (uint8_t(h.seq + 8) == t.seqEcho);
  }
  return p;
}

struct Sound {
  std::string path;
  Manifest manifest;
  Patch patch;
  bool fromZip = false;       // false when a bare payload file was loaded
  bool manifestCrcOk = false;
  bool payloadCrcOk = false;
};

// Accepts a .dn2pst container, or an already-extracted payload file.
inline Sound load(const std::string& path) {
  Sound s;
  s.path = path;
  Bytes file = readWholeFile(path);

  const bool isZip = file.size() >= 4 && file[0] == 'P' && file[1] == 'K';
  if (!isZip) {
    s.patch = parsePayload(std::move(file));
    s.manifest.payload = s.patch.name;
    return s;
  }

  s.fromZip = true;
  const std::vector<ZipEntry> entries = readZip(file);
  const ZipEntry* manifestEntry = nullptr;
  for (const ZipEntry& e : entries)
    if (e.name == "manifest.json") manifestEntry = &e;
  if (!manifestEntry) throw Error("no manifest.json in " + path);

  s.manifest = parseManifest(manifestEntry->data);
  s.manifestCrcOk = manifestEntry->crcOk;

  const ZipEntry* payloadEntry = nullptr;
  for (const ZipEntry& e : entries)
    if (e.name == s.manifest.payload) payloadEntry = &e;
  if (!payloadEntry) {  // fall back to "the entry that is not the manifest"
    for (const ZipEntry& e : entries)
      if (e.name != "manifest.json") payloadEntry = &e;
  }
  if (!payloadEntry)
    throw Error("payload entry \"" + s.manifest.payload + "\" not found in " +
                path);

  s.payloadCrcOk = payloadEntry->crcOk;
  s.patch = parsePayload(payloadEntry->data, &s.manifest.payload);
  return s;
}

}  // namespace dn2pst

#endif  // DN2PST_HPP_INCLUDED
