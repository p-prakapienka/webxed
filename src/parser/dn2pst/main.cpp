// dn2pst — read Elektron Digitone II .dn2pst sound files.
//
//   dn2pst <file...>      full dump of every file
//   dn2pst -t <file...>   one line per file (the regression table)
//   dn2pst -x <file...>   full dump plus a hex dump of the decompressed record
//   dn2pst -r <file...>   write the decompressed record to <file>.rec
#include "dn2pst.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace dn2pst;

static void dumpHex(const Bytes& b) {
  for (size_t i = 0; i < b.size(); i += 16) {
    std::printf("  %03zx  ", i);
    for (size_t k = 0; k < 16; ++k)
      if (i + k < b.size()) std::printf("%02x ", b[i + k]); else std::printf("   ");
    std::printf(" ");
    for (size_t k = 0; k < 16 && i + k < b.size(); ++k) {
      const uint8_t c = b[i + k];
      std::putchar(c >= 32 && c < 127 ? c : '.');
    }
    std::putchar('\n');
  }
}

static void full(const Patch& p) {
  std::printf("%s\n", p.file.c_str());
  std::printf("  name        \"%s\"\n", p.patchName.c_str());
  std::printf("  payload     %zu bytes compressed -> %zu decompressed"
              "  (declared %u)\n",
              p.payload.size(), p.record.size(), p.declaredLen());
  std::printf("  save count  %u\n", p.saveCounter());

  double b1, b2; bool bit0; ratioB(p, b1, b2, bit0);
  const auto& A = opARatios(); const auto& C = opCRatios();
  const uint16_t ai = p.u16(F_RATIO_A), ci = p.u16(F_RATIO_C);

  std::printf("  page 1\n");
  std::printf("    algorithm      %d\n", algorithm(p));
  std::printf("    op C ratio     %.2f  (index %u)\n",
              ci < C.size() ? C[ci] : -1.0, ci);
  std::printf("    op A ratio     %.2f  (index %u)\n",
              ai < A.size() ? A[ai] : -1.0, ai);
  std::printf("    op B ratios    %.2f / %.2f  (packed 0x%04X%s)\n",
              b1, b2, p.u16(F_RATIO_B), bit0 ? ", bit0 set" : "");
  std::printf("    harm           %+.2f\n", showTrunc(harm(p), 2));
  std::printf("    detune         %.2f\n",  showTrunc(detune(p), 2));
  std::printf("    feedback       %u\n",    p.u8(F_FEEDBACK));
  std::printf("    mix            %+d\n",   mix(p));
  std::printf("  page 2\n");
  std::printf("    op A  atk %3u  dec %3u  end %3u  lvl %3u\n",
              p.u16(F_A_ATTACK), p.u16(F_A_DECAY), p.u16(F_A_END), p.u16(F_A_LEVEL));
  std::printf("    op B  atk %3u  dec %3u  end %3u  lvl %3u\n",
              p.u16(F_B_ATTACK), p.u16(F_B_DECAY), p.u16(F_B_END), p.u16(F_B_LEVEL));
  std::printf("  page 3\n");
  std::printf("    op A  delay %3u  trig %s  reset %s\n", p.u16(F_A_DELAY),
              p.u16(F_A_TRIG) ? "on " : "off", p.u16(F_A_RESET) ? "on " : "off");
  std::printf("    op B  delay %3u  trig %s  reset %s\n", p.u16(F_B_DELAY),
              p.u16(F_B_TRIG) ? "on " : "off", p.u16(F_B_RESET) ? "on " : "off");
  std::printf("    phase reset    %u (%s)\n", p.u16(F_PHASE_RESET),
              phaseResetName(p.u16(F_PHASE_RESET)));
  std::printf("  page 4\n");
  std::printf("    ratio offsets  C %+.3f  A %+.3f  B1 %+.3f  B2 %+.3f\n",
              showFloor(ratioOffset(p, F_OFFSET_C), 3),
              showFloor(ratioOffset(p, F_OFFSET_A), 3),
              showFloor(ratioOffset(p, F_OFFSET_B1), 3),
              showFloor(ratioOffset(p, F_OFFSET_B2), 3));
  std::printf("    key track      A %u  B1 %u  B2 %u\n",
              p.u8(F_KEYTRACK_A), p.u8(F_KEYTRACK_B1), p.u8(F_KEYTRACK_B2));
  std::printf("  unidentified  0x074 = %u   0x092 = %u\n",
              p.u16(F_UNKNOWN_74), p.u16(F_UNKNOWN_92));
}

static void tableHeader() {
  std::printf("%-16s %5s %4s %6s %6s %6s %6s %7s %7s %5s %5s",
              "name", "bytes", "alg", "ratioC", "ratioA", "ratB1", "ratB2",
              "harm", "detune", "fdbk", "mix");
  std::printf(" %4s %4s %4s %4s %4s %4s %4s %4s",
              "Aatk", "Adec", "Aend", "Alvl", "Batk", "Bdec", "Bend", "Blvl");
  std::printf(" %4s %3s %3s %4s %3s %3s %4s",
              "Adly", "Atr", "Ars", "Bdly", "Btr", "Brs", "phrt");
  std::printf(" %7s %7s %7s %7s %3s %3s %3s %5s %5s\n",
              "offC", "offA", "offB1", "offB2", "ktA", "kB1", "kB2",
              "u074", "u092");
}

static void tableRow(const Patch& p) {
  double b1, b2; bool bit0; ratioB(p, b1, b2, bit0);
  const auto& A = opARatios(); const auto& C = opCRatios();
  const uint16_t ai = p.u16(F_RATIO_A), ci = p.u16(F_RATIO_C);
  std::printf("%-16s %5zu %4d %6.2f %6.2f %6.2f %6.2f %+7.2f %7.2f %5u %+5d",
              p.patchName.c_str(), p.payload.size(), algorithm(p),
              ci < C.size() ? C[ci] : -1.0, ai < A.size() ? A[ai] : -1.0, b1, b2,
              showTrunc(harm(p), 2), showTrunc(detune(p), 2),
              p.u8(F_FEEDBACK), mix(p));
  std::printf(" %4u %4u %4u %4u %4u %4u %4u %4u",
              p.u16(F_A_ATTACK), p.u16(F_A_DECAY), p.u16(F_A_END), p.u16(F_A_LEVEL),
              p.u16(F_B_ATTACK), p.u16(F_B_DECAY), p.u16(F_B_END), p.u16(F_B_LEVEL));
  std::printf(" %4u %3u %3u %4u %3u %3u %4u",
              p.u16(F_A_DELAY), p.u16(F_A_TRIG), p.u16(F_A_RESET),
              p.u16(F_B_DELAY), p.u16(F_B_TRIG), p.u16(F_B_RESET),
              p.u16(F_PHASE_RESET));
  std::printf(" %+7.3f %+7.3f %+7.3f %+7.3f %3u %3u %3u %5u %5u\n",
              showFloor(ratioOffset(p, F_OFFSET_C), 3),
              showFloor(ratioOffset(p, F_OFFSET_A), 3),
              showFloor(ratioOffset(p, F_OFFSET_B1), 3),
              showFloor(ratioOffset(p, F_OFFSET_B2), 3),
              p.u8(F_KEYTRACK_A), p.u8(F_KEYTRACK_B1), p.u8(F_KEYTRACK_B2),
              p.u16(F_UNKNOWN_74), p.u16(F_UNKNOWN_92));
}

int main(int argc, char** argv) {
  std::vector<std::string> files;
  const char* mode = "";
  for (int i = 1; i < argc; ++i) {
    if (argv[i][0] == '-' && argv[i][1] && !argv[i][2]) mode = argv[i];
    else files.push_back(argv[i]);
  }
  if (files.empty()) {
    std::fprintf(stderr,
      "usage: dn2pst [-t|-x|-r] <file...>\n"
      "  (none)  full dump      -t  one-line table\n"
      "  -x      dump + hex     -r  write <file>.rec\n");
    return 2;
  }
  if (!std::strcmp(mode, "-t")) tableHeader();
  int bad = 0;
  for (const auto& f : files) {
    Patch p;
    if (!load(f, p)) {
      std::fprintf(stderr, "%s: %s\n", f.c_str(), p.error.c_str());
      ++bad; continue;
    }
    if (!std::strcmp(mode, "-t")) { tableRow(p); continue; }
    full(p);
    if (!std::strcmp(mode, "-x")) { std::printf("  record\n"); dumpHex(p.record); }
    if (!std::strcmp(mode, "-r")) {
      const std::string o = f + ".rec";
      FILE* g = std::fopen(o.c_str(), "wb");
      if (g) { std::fwrite(p.record.data(), 1, p.record.size(), g); std::fclose(g);
               std::printf("  wrote %s\n", o.c_str()); }
    }
    std::putchar('\n');
  }
  return bad ? 1 : 0;
}
