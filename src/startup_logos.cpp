// Let the native movie loop stop and clean up a developer logo on confirm/Start.
#include "dreamcomp/hook.h"
#include "game_layout.h"

#include <cstdio>

DC_HOOK_ENTRY(logo_status) {
    // Only the status poll inside this executable profile's movie loop. Other decoder
    // callers and movies retain their native behavior.
    if ((c.pr & 0x1fffffffu) != army::layout::logo_status_caller) return false;
    constexpr std::uint32_t movie = army::layout::movie_state;
    const auto id = m.read32(movie + 56);
    const auto filename = m.read32(movie + 12);
    if (!((id == 1 && filename == army::layout::midway_filename) ||
          (id == 2 && filename == army::layout::saffire_filename))) return false;
    // The native input reader stores newly pressed, active-high buttons at +8.
    // Use an edge so holding a button cannot skip the following logo/title.
    constexpr std::uint32_t confirm_or_start = 0x04u | 0x08u;
    if (!(m.read32(army::layout::newly_pressed_buttons) & confirm_or_start)) return false;
    // Status 3 ends this iteration through the native stop/destroy/audio-restore
    // path. Leave the persistent movie-control flags intact for the next logo.
    c.r[0] = 3;
    std::printf("Army Men: skipped %s logo\n", id == 1 ? "Midway" : "Saffire");
    return true;
}
