// main.cpp — CLI for inspecting .dn2pst sound files.
//
//   build:  g++ -std=c++17 -O2 -Wall -Wextra main.cpp -lz -o dn2pst
//
//   dn2pst <file...>          detailed dump of each file
//   dn2pst -t <file...>       one-line-per-file table of parameter values
//   dn2pst -x <file...>       detailed dump plus a hex dump of the body
//   dn2pst -d <a> <b>         aligned byte diff of two files (offsets as body+N)
//   dn2pst -m <file...>       8-bit marker / 7-bit block map of the body
//   dn2pst -u <file...>       EXPERIMENTAL LZ decompression of the payload
//
// A value printed with a trailing '*' was not read from the file: the parameter
// has no entry, so its documented default is reported.
//
// Accepts .dn2pst containers or already-extracted payload files.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "dn2pst.hpp"

namespace {

void hexDump(const uint8_t* data, size_t size, size_t baseOffset) {
  for (size_t i = 0; i < size; i += 16) {
    const size_t n = std::min<size_t>(16, size - i);
    std::string ascii;
    for (size_t k = 0; k < n; ++k) {
      const uint8_t c = data[i + k];
      ascii += (c >= 0x20 && c < 0x7F) ? char(c) : '.';
    }
    std::printf("    %04zX  %-47s  %s\n", baseOffset + i,
                dn2pst::toHex(data + i, n).c_str(), ascii.c_str());
  }
}

void dumpOne(const dn2pst::Sound& s, bool withHex) {
  const dn2pst::Patch& p = s.patch;
  const dn2pst::Header& h = p.header;
  const dn2pst::Trailer& t = p.trailer;

  std::printf("%s\n", s.path.c_str());
  if (s.fromZip) {
    std::printf("  manifest   format=%s  type=%s  firmware=%s  payload=\"%s\"\n",
                s.manifest.formatVersion.c_str(), s.manifest.fileType.c_str(),
                s.manifest.firmwareVersion.c_str(), s.manifest.payload.c_str());
    std::printf("  zip crc    manifest=%s  payload=%s\n",
                s.manifestCrcOk ? "ok" : "MISMATCH",
                s.payloadCrcOk ? "ok" : "MISMATCH");
  } else {
    std::printf("  manifest   (none - bare payload file)\n");
  }
  std::printf("  name       \"%s\" (%zu chars) @ 0x%02zX\n", p.name.c_str(),
              p.name.size(), p.nameOffset);
  std::printf("  header     magic=%s%s  tag=\"%s\"  saveCounter=0x%02X  seq=0x%02X\n",
              dn2pst::toHex(h.magic.data(), 4).c_str(),
              h.magicOk ? "" : " (UNEXPECTED)", h.tag.c_str(), h.saveCounter,
              h.seq);
  std::printf("  nameField  %s   (tracks name length)\n",
              dn2pst::toHex(h.nameLenField).c_str());
  std::printf("  payload    %zu bytes, body %zu bytes @ 0x%02zX\n",
              p.raw.size(), p.bodySize(), p.bodyOffset);
  std::printf("  trailer    checksum=0x%08X  seqEcho=0x%02X (%s)  magic=%s\n",
              t.checksum, t.seqEcho, t.seqEchoOk ? "== seq+8" : "UNEXPECTED",
              t.magicOk ? "ok" : "MISMATCH");

  std::printf("  parameters\n");
  for (const dn2pst::ParamResult& r : dn2pst::decodeAll(p)) {
    if (r.usable()) {
      std::string shown;
      if (r.hasReal) {
        char buf[32];
        const double t = (r.real < 0 ? -1 : 1) *
                         std::floor(std::fabs(r.real) * 100.0) / 100.0;
        std::snprintf(buf, sizeof buf, r.signedValue ? "%+.2f" : "%.2f", t);
        shown = buf;
      } else {
        shown = std::to_string(r.value);
      }
      if (r.status == dn2pst::Status::AssumedDefault) shown += "*";
      std::printf("    %-14s %6s %-16s %s\n", r.id.c_str(), shown.c_str(),
                  r.valueLabel.empty() ? "" : ("(" + r.valueLabel + ")").c_str(),
                  r.formNote.c_str());
    } else if (r.status == dn2pst::Status::UnknownEncoding) {
      std::printf("    %-14s    ? %-16s %s @body+%zu: %s\n", r.id.c_str(), "",
                  r.formNote.c_str(), r.entryBodyOffset,
                  dn2pst::toHex(r.entryBytes).c_str());
    } else {
      std::printf("    %-14s    - %s\n", r.id.c_str(),
                  dn2pst::statusName(r.status));
    }
  }

  const std::vector<dn2pst::FieldResult> frs = dn2pst::extractAllFields(p);
  if (!frs.empty()) {
    std::printf("  fields\n");
    for (const dn2pst::FieldResult& f : frs) {
      if (f.ok())
        std::printf("    %-14s @0x%02zX (body+%zu)  %-22s %s\n", f.id.c_str(),
                    f.fileOffset, f.bodyOffset,
                    dn2pst::toHex(f.bytes).c_str(),
                    f.meaning.empty() ? "(pattern not in known list)"
                                      : f.meaning.c_str());
      else
        std::printf("    %-14s %s\n", f.id.c_str(),
                    dn2pst::statusName(f.status));
    }
  }

  if (withHex) {
    std::printf("  body hex\n");
    hexDump(p.body(), p.bodySize(), 0);
  }
  std::printf("\n");
}

// ---------------------------------------------------------------------------
// aligned byte diff of two payloads (LCS; payloads are a few hundred bytes)
// ---------------------------------------------------------------------------

std::string offsetLabel(const dn2pst::Patch& p, size_t off) {
  char buf[32];
  if (off >= p.bodyOffset)
    std::snprintf(buf, sizeof buf, "body+%-4zu", off - p.bodyOffset);
  else
    std::snprintf(buf, sizeof buf, "0x%02zX     ", off);
  return buf;
}

void diffTwo(const dn2pst::Sound& sa, const dn2pst::Sound& sb) {
  const dn2pst::Bytes& a = sa.patch.raw;
  const dn2pst::Bytes& b = sb.patch.raw;
  const size_t n = a.size(), m = b.size();

  // LCS table
  std::vector<std::vector<uint16_t>> L(n + 1, std::vector<uint16_t>(m + 1, 0));
  for (size_t i = n; i-- > 0;)
    for (size_t j = m; j-- > 0;)
      L[i][j] = (a[i] == b[j]) ? uint16_t(L[i + 1][j + 1] + 1)
                               : std::max(L[i + 1][j], L[i][j + 1]);

  std::printf("A: %-14s %3zu bytes   body @0x%02zX\n",
              sa.patch.name.c_str(), n, sa.patch.bodyOffset);
  std::printf("B: %-14s %3zu bytes   body @0x%02zX\n",
              sb.patch.name.c_str(), m, sb.patch.bodyOffset);

  size_t i = 0, j = 0;
  int hunks = 0;
  while (i < n || j < m) {
    if (i < n && j < m && a[i] == b[j]) { ++i; ++j; continue; }
    // collect one hunk of non-matching bytes on each side
    const size_t ai = i, bj = j;
    while (i < n || j < m) {
      if (i < n && j < m && a[i] == b[j]) break;
      if (j >= m || (i < n && L[i + 1][j] >= L[i][j + 1])) ++i;
      else ++j;
    }
    std::printf("  %s A: %-26s B: %s\n", offsetLabel(sa.patch, ai).c_str(),
                dn2pst::toHex(a.data() + ai, i - ai).c_str(),
                dn2pst::toHex(b.data() + bj, j - bj).c_str());
    ++hunks;
  }
  if (!hunks) std::printf("  (identical)\n");
}

void dumpMap(const dn2pst::Sound& s) {
  const dn2pst::Patch& p = s.patch;
  std::printf("%-14s payload %3zu  body %3zu\n", p.name.c_str(), p.raw.size(),
              p.bodySize());
  for (const dn2pst::Block& b : dn2pst::blockMap(p)) {
    if (b.isMarker)
      std::printf("  body+%-4zu marker  %02X\n", b.offset, b.marker);
    else
      std::printf("  body+%-4zu 7-bit   %3zu bytes  %s\n", b.offset, b.size,
                  dn2pst::toHex(p.body() + b.offset,
                                std::min<size_t>(b.size, 12))
                      .c_str());
  }
  std::printf("  body+%-4zu trailer %3zu bytes  %s\n",
              p.bodySize() - dn2pst::kTrailerSize, dn2pst::kTrailerSize,
              dn2pst::toHex(p.body() + p.bodySize() - dn2pst::kTrailerSize,
                            dn2pst::kTrailerSize)
                  .c_str());
  std::printf("\n");
}

void dumpTable(const std::vector<dn2pst::Sound>& sounds) {
  size_t nameWidth = 4;
  for (const dn2pst::Sound& s : sounds)
    nameWidth = std::max(nameWidth, s.patch.name.size());

  std::printf("%-*s  %5s", int(nameWidth), "name", "bytes");
  for (const dn2pst::ParamSpec& spec : dn2pst::registry())
    std::printf("  %14s", spec.id.c_str());
  for (const dn2pst::FieldSpec& spec : dn2pst::fields())
    std::printf("  %-22s", spec.id.c_str());
  std::printf("\n");

  for (const dn2pst::Sound& s : sounds) {
    std::printf("%-*s  %5zu", int(nameWidth), s.patch.name.c_str(),
                s.patch.raw.size());
    for (const dn2pst::ParamResult& r : dn2pst::decodeAll(s.patch)) {
      const char* star =
          (r.status == dn2pst::Status::AssumedDefault) ? "*" : "";
      if (r.usable() && !r.valueLabel.empty())
        std::printf("  %14s", (std::to_string(r.value) + star + "=" +
                               r.valueLabel).c_str());
      else if (r.usable())
        std::printf("  %14s", (std::to_string(r.value) + star).c_str());
      else if (r.status == dn2pst::Status::UnknownEncoding)
        std::printf("  %14s", ("?" + dn2pst::toHex(
                                        r.entryBytes.data(),
                                        std::min<size_t>(6, r.entryBytes.size()),
                                        ""))
                                  .c_str());
      else
        std::printf("  %14s", "-");
    }
    for (const dn2pst::FieldResult& f : dn2pst::extractAllFields(s.patch)) {
      if (!f.ok()) std::printf("  %-22s", "-");
      else if (!f.meaning.empty()) std::printf("  %-22s", f.meaning.c_str());
      else std::printf("  %-22s", ("?" + dn2pst::toHex(f.bytes, "")).c_str());
    }
    std::printf("\n");
  }
}

}  // namespace

int main(int argc, char** argv) {
  bool table = false, withHex = false, diff = false, map = false, decomp = false;
  std::vector<std::string> paths;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    if (a == "-t" || a == "--table") table = true;
    else if (a == "-d" || a == "--diff") diff = true;
    else if (a == "-m" || a == "--map") map = true;
    else if (a == "-u" || a == "--decompress") decomp = true;
    else if (a == "-x" || a == "--hex") withHex = true;
    else if (a == "-h" || a == "--help") { paths.clear(); break; }
    else paths.push_back(a);
  }
  if (paths.empty()) {
    std::fprintf(stderr,
                 "usage: dn2pst [-t|--table] [-x|--hex] <file.dn2pst ...>\n"
                 "       dn2pst -d|--diff <a.dn2pst> <b.dn2pst>\n"
                 "       dn2pst -m|--map <file.dn2pst ...>\n");
    return 2;
  }

  std::vector<dn2pst::Sound> sounds;
  int failures = 0;
  for (const std::string& path : paths) {
    try {
      sounds.push_back(dn2pst::load(path));
    } catch (const std::exception& e) {
      std::fprintf(stderr, "%s: %s\n", path.c_str(), e.what());
      ++failures;
    }
  }

  if (decomp) {
    for (const dn2pst::Sound& s : sounds) {
      const dn2pst::Decompressed u = dn2pst::decompress(s.patch);
      std::printf("%-14s compressed %3zu -> %3zu bytes, %zu match tokens, "
                  "%zu literal fallbacks%s\n",
                  s.patch.name.c_str(), s.patch.raw.size(), u.data.size(),
                  u.tokensRead, u.literalFallbacks,
                  u.literalFallbacks ? "  (EXPERIMENTAL - see header)" : "");
      hexDump(u.data.data(), u.data.size(), 0);
      std::printf("\n");
    }
  } else if (map) {
    for (const dn2pst::Sound& s : sounds) dumpMap(s);
  } else if (diff) {
    if (sounds.size() != 2) {
      std::fprintf(stderr, "--diff needs exactly two readable files\n");
      return 2;
    }
    diffTwo(sounds[0], sounds[1]);
  } else if (table) {
    dumpTable(sounds);
  } else {
    for (const dn2pst::Sound& s : sounds) dumpOne(s, withHex);
  }

  return failures ? 1 : 0;
}
