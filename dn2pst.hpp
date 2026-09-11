// dn2pst.hpp — reader for Elektron Digitone II .dn2pst sound files (FW 1.10E)
//
// ===========================================================================
// THE FORMAT, SOLVED
// ===========================================================================
//
// .dn2pst is a ZIP with two entries: manifest.json, and a binary payload whose
// ZIP entry name is the patch name. The payload is:
//
//   0x00  AC 11 D3 03         magic
//   0x04  02                  \ constants written by the firmware's header
//   0x05  00 05               |  builder; see "provenance" below
//   0x07  00 0F               /
//   0x09  "0050"              ASCII tag
//   0x0D  u32 BE              3
//   0x11  u32 BE              2
//   0x15  u32 BE              save counter (decrements once per save)
//   0x19  u32 BE              UNCOMPRESSED RECORD LENGTH (364 for a sound)
//   0x1D  u8, u8              1, 12
//   0x23  ...                 LZ4 BLOCK STREAM
//   tail  u32 checksum, 00 00 00, seq+8, AA A1 DA AA
//
// THE COMPRESSION IS PLAIN LZ4 BLOCK FORMAT. Decompress from payload 0x23 for
// exactly the length at payload 0x19. Verified byte-exact on all 95 samples.
// The firmware confirms it: the OS image contains the C++ symbols
// "Lz4StreamCompressor" / "Lz4StreamDecompressor" and the LZ4 frame error
// strings ERROR_compressionLevel_invalid / ERROR_decompressionFailed.
//
// The decompressed record is 364 bytes, fixed layout, no compression, no
// surprises:
//
//   0x005  BE EF BA CE        storage-blob start magic
//   0x011  char[16]           patch name, NUL padded
//   0x062  ...                the parameter block, mapped below
//   0x168  BA CE F0 0C        storage-blob end magic
//
// Every parameter sits at a fixed offset. The map below was produced by
// decompressing all 95 samples and diffing each single-parameter sample
// against an init save, so each entry is a direct measurement.
//
// VALUE ENCODINGS
//   plain        u16 BE, 0..127
//   algorithm    u16 BE, stored as alg - 1
//   ratio        u16 BE index into a per-parameter table (op A 36, op C 19)
//   op B pair    u16 BE at an ODD offset; packed >> 1 == 19*b2 + b1, both
//                indexing op C's coarse table. Bit 0 is still unexplained.
//   harm/detune  u8 integer + u8 1/256 fraction. harm's integer is biased by
//                63, detune's by 0. Value = (int - bias) + frac/256, and the
//                display TRUNCATES TOWARD ZERO: harm -4.707 shows as -4.70.
//   mix          u16 BE, value + 64
//   ratio offset u16 BE at an ODD offset, Q1.14: (raw - 16384) / 16384, so
//                raw 0 = -1.000 and raw 32767 = +0.999, matching the range on
//                the device. The display FLOORS to three decimals (note: that
//                differs from harm/detune, which truncate toward zero - both
//                rules are measured, not assumed).
//
// PROVENANCE. The layout above was recovered from samples. It was then
// cross-checked against the Digitone II OS image (1.10D), decompressed with
// github.com/mischa85/elektron-firmware-tool: the firmware's own FM tone
// parameter table lists the parameters in the same order, "Ratio B" really is
// one parameter, and the ratio-offset display format strings ("+0." "-0."
// "-1.000" "0.000" "%s%03d") match the Q1.14 reading. The firmware also
// contains the header-builder function, whose immediates are exactly the
// constants at payload 0x00..0x1E. No firmware code or content is reproduced
// here - only the factual structures needed to read the file format.
//
// HISTORY. Before the compression was identified, this reader worked by
// pattern-matching anchors in the *compressed* bytes. That approach produced
// several readings that later had to be retracted (see git history and
// SAMPLES.md). It is gone: everything below reads the decompressed record.
// ===========================================================================

#ifndef DN2PST_HPP
#define DN2PST_HPP

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <zlib.h>

namespace dn2pst {

using Bytes = std::vector<uint8_t>;

// ---------------------------------------------------------------- minimal ZIP
struct ZipEntry { std::string name; Bytes data; };

inline uint16_t rd16(const uint8_t* p) { return uint16_t(p[0] | (p[1] << 8)); }
inline uint32_t rd32(const uint8_t* p) {
  return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) |
         (uint32_t(p[3]) << 24);
}

// Reads the central directory and inflates every entry. Stored and deflated
// entries are both handled; anything else is skipped.
inline bool readZip(const Bytes& z, std::vector<ZipEntry>& out) {
  if (z.size() < 22) return false;
  size_t eocd = 0; bool found = false;
  for (size_t i = z.size() - 22; ; --i) {
    if (rd32(&z[i]) == 0x06054b50) { eocd = i; found = true; break; }
    if (i == 0) break;
  }
  if (!found) return false;
  const uint16_t n = rd16(&z[eocd + 10]);
  size_t cd = rd32(&z[eocd + 16]);
  for (uint16_t k = 0; k < n && cd + 46 <= z.size(); ++k) {
    if (rd32(&z[cd]) != 0x02014b50) return false;
    const uint16_t method = rd16(&z[cd + 10]);
    const uint32_t csize  = rd32(&z[cd + 20]);
    const uint32_t usize  = rd32(&z[cd + 24]);
    const uint16_t nlen   = rd16(&z[cd + 28]);
    const uint16_t elen   = rd16(&z[cd + 30]);
    const uint16_t clen   = rd16(&z[cd + 32]);
    const uint32_t lho    = rd32(&z[cd + 42]);
    ZipEntry e;
    e.name.assign(reinterpret_cast<const char*>(&z[cd + 46]), nlen);
    if (lho + 30 > z.size()) return false;
    const size_t dstart = lho + 30 + rd16(&z[lho + 26]) + rd16(&z[lho + 28]);
    if (dstart + csize > z.size()) return false;
    if (method == 0) {
      e.data.assign(z.begin() + dstart, z.begin() + dstart + csize);
    } else if (method == 8) {
      e.data.resize(usize);
      z_stream s{}; s.next_in = const_cast<Bytes::value_type*>(&z[dstart]);
      s.avail_in = csize; s.next_out = e.data.data(); s.avail_out = usize;
      if (inflateInit2(&s, -15) != Z_OK) return false;
      const int r = inflate(&s, Z_FINISH); inflateEnd(&s);
      if (r != Z_STREAM_END) return false;
    } else { cd += 46 + nlen + elen + clen; continue; }
    out.push_back(std::move(e));
    cd += 46 + nlen + elen + clen;
  }
  return true;
}

// ------------------------------------------------------------- LZ4 block
// Standard LZ4 block format. Stops once `limit` bytes have been produced,
// which is what the payload's length field tells us.
inline bool lz4Block(const uint8_t* src, size_t srclen, size_t pos, size_t limit,
                     Bytes& out) {
  out.clear(); out.reserve(limit);
  while (pos < srclen && out.size() < limit) {
    const uint8_t tok = src[pos++];
    size_t ll = tok >> 4;
    if (ll == 15) {
      for (;;) { if (pos >= srclen) return false; uint8_t b = src[pos++];
                 ll += b; if (b != 255) break; }
    }
    if (pos + ll > srclen) return false;
    out.insert(out.end(), src + pos, src + pos + ll); pos += ll;
    if (out.size() >= limit) break;
    if (pos + 2 > srclen) return false;
    const size_t off = size_t(src[pos]) | (size_t(src[pos + 1]) << 8); pos += 2;
    if (off == 0 || off > out.size()) return false;
    size_t ml = tok & 0x0F;
    if (ml == 15) {
      for (;;) { if (pos >= srclen) return false; uint8_t b = src[pos++];
                 ml += b; if (b != 255) break; }
    }
    ml += 4;
    const size_t start = out.size() - off;
    for (size_t i = 0; i < ml; ++i) out.push_back(out[start + i]);
  }
  return out.size() == limit;
}

// ------------------------------------------------------------- ratio tables
inline const std::vector<double>& opCRatios() {           // 19 entries, coarse
  static const std::vector<double> t = [] {
    std::vector<double> v{0.25, 0.50, 0.75, 1.00};
    for (int i = 2; i <= 16; ++i) v.push_back(double(i));
    return v;
  }();
  return t;
}
inline const std::vector<double>& opARatios() {           // 36 entries, fine
  static const std::vector<double> t = [] {
    std::vector<double> v;
    for (int i = 1; i <= 20; ++i) v.push_back(0.25 * i);        // 0.25 .. 5.00
    for (int i = 0; i < 10; ++i) v.push_back(5.5 + 0.5 * i);    // 5.50 .. 10.0
    for (int i = 11; i <= 16; ++i) v.push_back(double(i));      // 11 .. 16
    return v;
  }();
  return t;
}

// ------------------------------------------------------------- the record
enum : size_t {
  REC_LEN         = 364,
  OFF_BLOB_MAGIC  = 0x005,   // BE EF BA CE
  OFF_NAME        = 0x011,   // char[16]
  OFF_END_MAGIC   = 0x168,   // BA CE F0 0C
};

// Field offsets in the decompressed record. Every one measured by diffing a
// single-parameter sample against an init save. Widths are as measured too:
// most fields are u16 BE on an even grid, but the packed op B pair and the
// four ratio offsets sit at ODD offsets, and key track is a single byte.
enum : size_t {
  F_ALGORITHM   = 0x062,  // u16, stored as alg - 1
  F_RATIO_C     = 0x064,  // u16 index, op C table
  F_RATIO_A     = 0x066,  // u16 index, op A table
  F_RATIO_B     = 0x069,  // u16 ODD, packed pair
  F_HARM_INT    = 0x06b,  // u8 + 63
  F_HARM_FRAC   = 0x06c,  // u8 /256
  F_DETUNE_INT  = 0x06d,  // u8
  F_DETUNE_FRAC = 0x06e,  // u8 /256
  F_FEEDBACK    = 0x06f,  // u8
  F_MIX         = 0x070,  // u16 + 64
  F_PHASE_RESET = 0x072,  // u16 enum
  F_UNKNOWN_74  = 0x074,  // u16, 0 pre-H213 and 64 after; not identified
  F_A_ATTACK    = 0x076,  F_A_DECAY = 0x078, F_A_END = 0x07a, F_A_LEVEL = 0x07c,
  F_B_ATTACK    = 0x07e,  F_B_DECAY = 0x080, F_B_END = 0x082, F_B_LEVEL = 0x084,
  F_A_DELAY     = 0x086,  F_A_TRIG  = 0x088, F_A_RESET = 0x08a,
  F_B_DELAY     = 0x08c,  F_B_TRIG  = 0x08e, F_B_RESET = 0x090,
  F_UNKNOWN_92  = 0x092,  // u16, 0 pre-H213 and 127 after; not identified
  F_OFFSET_C    = 0x0a5,  // u16 ODD, Q1.14
  F_OFFSET_A    = 0x0a7,
  F_OFFSET_B1   = 0x0a9,
  F_OFFSET_B2   = 0x0ab,
  F_KEYTRACK_A  = 0x0ad,  // u8
  F_KEYTRACK_B1 = 0x0af,  // u8
  F_KEYTRACK_B2 = 0x0b1,  // u8
};

struct Patch {
  std::string file, patchName, manifest;
  Bytes payload;     // the compressed payload entry
  Bytes record;      // 364 decompressed bytes
  bool  ok = false;
  std::string error;

  uint8_t  u8 (size_t o) const { return record[o]; }
  uint16_t u16(size_t o) const { return uint16_t((record[o] << 8) | record[o + 1]); }

  uint32_t saveCounter() const { return be32(payload, 0x15); }
  uint32_t declaredLen() const { return be32(payload, 0x19); }

  static uint32_t be32(const Bytes& b, size_t o) {
    return (uint32_t(b[o]) << 24) | (uint32_t(b[o + 1]) << 16) |
           (uint32_t(b[o + 2]) << 8) | uint32_t(b[o + 3]);
  }
};

inline bool load(const std::string& path, Patch& p) {
  p.file = path;
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) { p.error = "cannot open"; return false; }
  Bytes z; uint8_t buf[65536]; size_t n;
  while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) z.insert(z.end(), buf, buf + n);
  std::fclose(f);

  std::vector<ZipEntry> es;
  if (!readZip(z, es)) { p.error = "not a readable zip"; return false; }
  for (auto& e : es) {
    if (e.name == "manifest.json")
      p.manifest.assign(e.data.begin(), e.data.end());
    else { p.patchName = e.name; p.payload = std::move(e.data); }
  }
  if (p.payload.size() < 0x30) { p.error = "payload too small"; return false; }
  if (!(p.payload[0] == 0xAC && p.payload[1] == 0x11 &&
        p.payload[2] == 0xD3 && p.payload[3] == 0x03)) {
    p.error = "bad payload magic"; return false;
  }
  const uint32_t want = p.declaredLen();
  if (!lz4Block(p.payload.data(), p.payload.size(), 0x23, want, p.record)) {
    p.error = "LZ4 decompression failed"; return false;
  }
  p.ok = true;
  return true;
}

// ------------------------------------------------------------- accessors
inline double harm(const Patch& p) {
  return double(int(p.u8(F_HARM_INT)) - 63) + double(p.u8(F_HARM_FRAC)) / 256.0;
}
inline double detune(const Patch& p) {
  return double(p.u8(F_DETUNE_INT)) + double(p.u8(F_DETUNE_FRAC)) / 256.0;
}
inline int mix(const Patch& p) { return int(p.u16(F_MIX)) - 64; }
inline int algorithm(const Patch& p) { return int(p.u16(F_ALGORITHM)) + 1; }

inline double ratioOffset(const Patch& p, size_t off) {
  return (double(p.u16(off)) - 16384.0) / 16384.0;
}

inline void ratioB(const Patch& p, double& b1, double& b2, bool& bit0) {
  const uint16_t packed = p.u16(F_RATIO_B);
  bit0 = (packed & 1) != 0;
  const int v = packed >> 1;
  const auto& t = opCRatios();
  const int i1 = v % 19, i2 = v / 19;
  b1 = (i1 >= 0 && size_t(i1) < t.size()) ? t[i1] : -1.0;
  b2 = (i2 >= 0 && size_t(i2) < t.size()) ? t[i2] : -1.0;
}

inline const char* phaseResetName(int v) {
  switch (v) {
    case 0: return "all";  case 1: return "off"; case 2: return "C";
    case 3: return "A+B";  case 4: return "A+B2";
    default: return "?";
  }
}

// The device truncates toward zero for harm/detune and floors for the ratio
// offsets. Both are measured against the screen, so they are kept distinct.
inline double showTrunc(double v, int dp) {
  double s = 1.0; for (int i = 0; i < dp; ++i) s *= 10.0;
  return static_cast<double>(static_cast<long long>(v * s)) / s;
}
inline double showFloor(double v, int dp) {
  double s = 1.0; for (int i = 0; i < dp; ++i) s *= 10.0;
  double x = v * s; long long f = (long long)x; if (x < 0 && double(f) != x) --f;
  return double(f) / s;
}

}  // namespace dn2pst

#endif  // DN2PST_HPP
