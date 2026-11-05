// Restore a rotor loop for the otherwise silent Snow drop-off cinematic.
#include "dreamcomp/hook.h"
#include "dream/runtime/host_ext.h"
#include "dream/runtime/system.h"

namespace {
bool native_snow_soundtrack = false;
struct SnowSoundtrack final : dream::host::Extension {
    const char* name() const override { return "Army Men Snow soundtrack selection"; }
    void on_start(dream::System& sys) override {
        // The verified USA GD-ROM executable has this PVD FAD and includes CIN8_1.STR.
        // The CDI copy omits that stream; only that copy needs our replacement rotor cue.
        native_snow_soundtrack = sys.memory.read32(0x8c135eb8u) == 45166u;
    }
    void on_stop(dream::System&, const char*) override { native_snow_soundtrack = false; }
};
SnowSoundtrack snow_soundtrack;
dream::host::Register register_snow_soundtrack(snow_soundtrack);
} // namespace

DC_HOOK_ENTRY(snow_intro_audio) {
    if (native_snow_soundtrack) return false;
    // The native cinematic updater runs after the loaded ELF header is installed.
    // Restrict this edit to Snow Cnm00 and its known single helicopter cue.
    if (m.read32(0x8c18ed20u) != 8) return false;
    const auto header = m.read32(0x8c17e338u);
    const auto in_ram = [](std::uint32_t address, std::uint32_t size) {
        return (address & 0xff000000u) == 0x8c000000u &&
               (address & 0x00ffffffu) <= 0x01000000u - size;
    };
    if (!in_ram(header, 104) || m.read32(header) != 0 ||
        m.read32(header + 4) != 10533 || m.read32(header + 32) != 1)
        return false;
    const auto cue = m.read32(header + 36);
    if (!in_ram(cue, 40) || m.read32(cue) != 0 || (m.read32(cue + 4) != 4000 && m.read32(cue + 4) != 6000) ||
        m.read16(cue + 14) != 100 || m.read32(cue + 8) != 5)
        return false;
    if (m.read32(cue + 16) == 1 && m.read16(cue + 12) == 23) {
        // Sample 33 is the game's looping helicopter engine (native vehicle
        // update 0x0c04f378); the original cue names a short non-looping effect.
        // Clear positional attenuation: the authored source is at world origin,
        // far from Snow's helicopter. Keep it running until the helicopter leaves the screen;
        // preserve the authored volume and native handle.
        m.write32(cue + 4, 6000);
        m.write16(cue + 12, 33);
        m.write32(cue + 16, 0);
    }
    // Native cleanup stops the loop at six seconds and on early cinematic exit.
    return false;
}

DC_HOOK_ENTRY(snow_intro_play) {
    if (native_snow_soundtrack) return false;
    // Cinematics mute the normal SFX bus because their audio usually comes from
    // a streamed soundtrack. Let only this restored cue use the saved SFX level.
    if ((c.pr & 0x1fffffffu) != 0x0c055c4cu || c.r[5] != 33 || c.r[6] != 0 ||
        m.read32(0x8c18ed20u) != 8)
        return false;
    const auto header = m.read32(0x8c17e338u);
    if ((header & 0xff000000u) != 0x8c000000u ||
        (header & 0xffffffu) > 0xffff98u || m.read32(header) != 0 ||
        m.read32(header + 4) != 10533 || m.read32(header + 32) != 1)
        return false;
    const auto cue = m.read32(header + 36);
    if ((cue & 0xff000000u) != 0x8c000000u || (cue & 0xffffffu) > 0xffffd8u ||
        m.read16(cue + 12) != 33 || m.read32(cue + 16) != 0 ||
        m.read32(0x8c17e3c0u) > 6000)
        return false;
    const auto level = m.read32(0x8c17e4e8u);
    if (level > 0 && level <= 10)
        c.r[6] = 50 + 5 * level; // native volume curve for this 100-volume cue
    return false;
}
