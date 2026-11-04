#include "widescreen.h"
#include "dreamcomp/hook.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>

namespace {
constexpr float original_aspect = 4.0f / 3.0f;
float target_aspect = original_aspect;
bool changed_aspect = false;
std::uint32_t submitted_projection = 0;
float original_projection_x = 0.0f;
// The game's own value at camera+0xe8 is the viewport's aspect: 4:3 for the full
// screen, 8:3 for a top/bottom split. Keep each camera's original and scale it by
// the widescreen factor, rather than replacing a split viewport with 16:9.
std::unordered_map<std::uint32_t, float> camera_original_aspect;
std::unordered_map<std::uint32_t, float> camera_written_aspect;
float traced_aspect = -1.0f;
}

void army_set_aspect(dream::System&, float aspect) {
    const float next = std::clamp(aspect, original_aspect, 16.0f / 9.0f);
    if (next != target_aspect) {
        camera_original_aspect.clear();
        camera_written_aspect.clear();
    }
    target_aspect = next;
    if (target_aspect > original_aspect + 0.001f) changed_aspect = true;
}

void army_camera_aspect(dream::Memory& memory, std::uint32_t camera) {
    // This aspect also expands the game's visibility/culling bounds. Scale the
    // game's own viewport aspect so a split-screen viewport keeps its wider shape.
    if (!changed_aspect) return;
    const float existing = dreamcomp::read_f32(memory, camera + 0xe8);
    if (!(existing > 0.1f && existing < 10.0f)) {
        dreamcomp::write_f32(memory, camera + 0xe8, target_aspect);
        return;
    }
    const auto written = camera_written_aspect.find(camera);
    if (written == camera_written_aspect.end() || existing != written->second)
        camera_original_aspect[camera] = existing;  // the game set a new viewport
    const float scaled = camera_original_aspect[camera] * (target_aspect / original_aspect);
    dreamcomp::write_f32(memory, camera + 0xe8, scaled);
    camera_written_aspect[camera] = scaled;
    if (std::getenv("ARMY_TRACE_ASPECT") && scaled != traced_aspect) {
        std::printf("aspect camera=%08x original=%g scaled=%g target=%g\n", camera,
                    camera_original_aspect[camera], scaled, target_aspect);
        traced_aspect = scaled;
    }
}

DC_HOOK_ENTRY(projection_submit) {
    // fn_0c057150 has just rebuilt this matrix, with horizontal focal scale at +0.
    // Patch before the copy into renderer globals, after that recomputation.
    if ((c.pr & 0x1fffffffu) == 0x0c05720au && target_aspect > original_aspect + 0.001f) {
        const auto address = c.r[4];
        const float scale = dreamcomp::read_f32(m, address);
        submitted_projection = address;
        original_projection_x = scale;
        dreamcomp::write_f32(m, address, scale * original_aspect / target_aspect);
    } else {
        submitted_projection = 0;
    }
    return false;
}

DC_HOOK_EXIT(projection_submit) {
    // The game shares this matrix with its own targeting and visibility code.
    // Leave the widened value in place only while the renderer copies it.
    if (submitted_projection != 0) {
        dreamcomp::write_f32(m, submitted_projection, original_projection_x);
        submitted_projection = 0;
    }
}
