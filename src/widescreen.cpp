#include "widescreen.h"
#include "dreamcomp/hook.h"

#include <algorithm>
#include <cmath>

namespace {
constexpr float original_aspect = 4.0f / 3.0f;
float target_aspect = original_aspect;
bool changed_aspect = false;
std::uint32_t submitted_projection = 0;
float original_projection_x = 0.0f;
}

void army_set_aspect(dream::System&, float aspect) {
    target_aspect = std::clamp(aspect, original_aspect, 16.0f / 9.0f);
    if (target_aspect > original_aspect + 0.001f) changed_aspect = true;
}

void army_camera_aspect(dream::Memory& memory, std::uint32_t camera) {
    // This aspect also expands the game's visibility/culling bounds. Keep it in
    // step with the widened renderer so objects remain visible at the screen edges.
    if (changed_aspect)
        dreamcomp::write_f32(memory, camera + 0xe8, target_aspect);
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
