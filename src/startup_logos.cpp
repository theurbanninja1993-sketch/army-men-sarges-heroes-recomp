// Let the native movie loop stop and clean up a developer logo on confirm/Start.
#include "dreamcomp/hook.h"

#include <cstdio>

DC_HOOK_ENTRY(logo_status) {
    // Only the status poll inside fn_0c072f78's movie loop. Other decoder
    // callers and movies retain their native behavior.
    if ((c.pr & 0x1fffffffu) != 0x0c073074u) return false;
    constexpr std::uint32_t movie = 0x8c0f01e0u;
    const auto id = m.read32(movie + 56);
    const auto filename = m.read32(movie + 12);
    if (!((id == 1 && filename == 0x8c0f0220u) ||
          (id == 2 && filename == 0x8c0f022fu))) return false;
    // The native input reader stores newly pressed, active-high buttons at +8.
    // Use an edge so holding a button cannot skip the following logo/title.
    constexpr std::uint32_t confirm_or_start = 0x04u | 0x08u;
    if (!(m.read32(0x8c1fdbf8u) & confirm_or_start)) return false;
    // Status 3 ends this iteration through the native stop/destroy/audio-restore
    // path. Leave the persistent movie-control flags intact for the next logo.
    c.r[0] = 3;
    std::printf("Army Men: skipped %s logo\n", id == 1 ? "Midway" : "Saffire");
    return true;
}
