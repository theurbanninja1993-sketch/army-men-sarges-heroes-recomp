// Synthetic camera clones and projection copies; no disc data or save files.
#undef NDEBUG
#include <cassert>
#include <cmath>
#include <cstdio>
#include "dreamcomp/hook.h"
#include "dream/runtime/system.h"
#include "../src/game_layout.h"
#include "../src/widescreen.h"
namespace dream::gen {
bool dream_hook_entry_projection_submit(dream::sh4::Ctx&, dream::Memory&);
void dream_hook_exit_projection_submit(dream::sh4::Ctx&, dream::Memory&);
}
int main() {
    dream::System sys;
    auto& m = sys.memory;
    constexpr std::uint32_t a = 0x8c300000, b = 0x8c301000, split = 0x8c302000;
    auto equal = [](float x, float y) { assert(std::abs(x - y) < 0.0001f); };
    auto aspect = [&](std::uint32_t camera) { return dreamcomp::read_f32(m, camera + 0xe8); };
    dreamcomp::write_f32(m, a + 0xe8, 4.f / 3.f);
    dreamcomp::write_f32(m, split + 0xe8, 8.f / 3.f);
    army_set_aspect(sys, 16.f / 9.f);
    army_camera_aspect(m, a);
    equal(aspect(a), 16.f / 9.f);
    dreamcomp::write_f32(m, b + 0xe8, aspect(a));
    army_camera_aspect(m, b);
    equal(aspect(b), 16.f / 9.f);
    army_camera_aspect(m, split);
    equal(aspect(split), 32.f / 9.f);
    army_set_aspect(sys, 4.f / 3.f);
    equal(aspect(a), 4.f / 3.f); equal(aspect(b), 4.f / 3.f); equal(aspect(split), 8.f / 3.f);
    army_camera_aspect(m, b);
    equal(aspect(b), 4.f / 3.f);
    army_set_aspect(sys, 1.5f); army_camera_aspect(m, a); equal(aspect(a), 1.5f);
    army_set_aspect(sys, 16.f / 9.f); army_camera_aspect(m, a); equal(aspect(a), 16.f / 9.f);
    constexpr std::uint32_t matrix = 0x8c303000, submitted = 0x8c304000;
    dreamcomp::write_f32(m, matrix, 240.f);
    sys.ctx.pr = army::layout::projection_caller;
    sys.ctx.r[4] = matrix;
    assert(!dream::gen::dream_hook_entry_projection_submit(sys.ctx, m));
    m.write32(submitted, m.read32(matrix));
    dream::gen::dream_hook_exit_projection_submit(sys.ctx, m);
    equal(dreamcomp::read_f32(m, submitted), 180.f);
    equal(dreamcomp::read_f32(m, matrix), 240.f);
    std::puts("PASS: cloned cameras, aspect switching, split viewports and projection restoration");
}
