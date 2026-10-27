// Replay an F11 capture without booting the game or opening a window.
// Usage: armymensh_render_capture FRAME.ta FRAME.vram OUT.png [--wrap] [--point] [--snap]
//        [--previous OLDER.ta] [--blend-t FRACTION] [--inspect] [--wide [--hud OVERRIDES.ini]]
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>
#include <array>
#include <bit>
#include <map>
#include "dream/render/background.h"
#include "dream/render/display_list.h"
#include "dream/render/fog.h"
#include "dream/render/interpolate.h"
#include "dream/render/png.h"
#include "dream/render/tsp.h"
#include "dreamcomp/hud.h"
#include "dream/render/vk/offscreen.h"

template<class T> std::vector<T> read(const std::string& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) return {};
    const auto bytes = static_cast<std::size_t>(in.tellg());
    std::vector<T> result(bytes / sizeof(T));
    in.seekg(0);
    in.read(reinterpret_cast<char*>(result.data()), static_cast<std::streamsize>(result.size() * sizeof(T)));
    return result;
}

int main(int argc, char** argv) {
    if (argc < 4) { std::fprintf(stderr, "usage: %s FRAME.ta FRAME.vram OUT.png [--wrap] [--point] [--snap]\n", argv[0]); return 2; }
    const auto ta = read<std::uint32_t>(argv[1]);
    const auto vram = read<std::uint8_t>(argv[2]);
    const auto regs = read<std::uint32_t>(std::string(argv[2]) + ".regs");
    if (ta.empty() || vram.size() != 8388608 || regs.size() != 2048) return 2;
    dream::render::DisplayList decoder;
    decoder.feed_stream(ta.data(), ta.size());
    auto& frame = decoder.frame();
    dream::render::add_background(frame, regs.data(), vram.data(), vram.size());
    if (argc > 4 && std::string(argv[4]) == "--inspect") {
        for (unsigned l=0;l<frame.lists.size();++l) for (const auto& p : frame.lists[l]) {
            bool flat=true; float x0=1e9,y0=1e9,x1=-1e9,y1=-1e9;
            for (unsigned k=0;k<p.count;++k) { const auto& v=frame.vertices[p.first+k];
                flat &= std::abs(v.z-frame.vertices[p.first].z)<1e-6;
                x0=std::min(x0,v.x);x1=std::max(x1,v.x);y0=std::min(y0,v.y);y1=std::max(y1,v.y);
            }
            if (flat) std::printf("list%u first%u n%u box %.4f %.4f %.4f %.4f z%.8f pcw%08x isp%08x tsp%08x tcw%08x color%08x\n",l,p.first,p.count,x0,y0,x1,y1,frame.vertices[p.first].z,p.pcw,p.isp,p.tsp,p.tcw,frame.vertices[p.first].base);
        }
        return 0;
    }
    bool wrap = false, point = false, snap = false;
    for (int i = 4; i < argc; ++i) {
        wrap |= std::string(argv[i]) == "--wrap";
        point |= std::string(argv[i]) == "--point";
        snap |= std::string(argv[i]) == "--snap";
    }
    bool wide = false;
    for (int i=4;i<argc;++i) wide |= std::string(argv[i]) == "--wide";
    dreamcomp::hud::Overrides overrides;
    for (int i=4;i+1<argc;++i) if (std::string(argv[i]) == "--hud") {
        std::string error;
        if (!overrides.load(argv[i+1], &error)) { std::fprintf(stderr, "%s\n",error.c_str()); return 2; }
    }
    auto prepare = [&](dream::render::Frame& input) {
        for (auto& list : input.lists) for (auto& polygon : list) {
            if (point) polygon.tsp &= ~(3u << 13);
            for (unsigned i = 0; i < polygon.count; ++i) {
                auto& v = input.vertices[polygon.first + i];
                // A period of two preserves repeat and mirror addressing but defeats the
                // renderer's unit-square clamp heuristic, for an isolated diagnostic comparison.
                if (wrap && (polygon.pcw & 8)) {
                    if (!dream::render::tsp_clamp_u(polygon.tsp)) v.u += 2;
                    if (!dream::render::tsp_clamp_v(polygon.tsp)) v.v += 2;
                }
                if (snap) { v.x = std::round(v.x * 16) / 16; v.y = std::round(v.y * 16) / 16; }
            }
        }
        // Live interpolation receives both frames after HUD correction. Correcting
        // only the final blended result can change classifications and hide artifacts.
        if (wide && !dreamcomp::hud::pillarbox_2d(input,16.f/9.f)) {
            dreamcomp::PortInfo::HudRule rule; rule.enabled=true; rule.polygons=true;
            dreamcomp::hud::Stats stats;
            dreamcomp::hud::correct(input,rule,16.f/9.f,true,true,overrides,nullptr,stats);
        }
    };
    prepare(frame);
    for (int i = 4; i + 1 < argc; ++i) if (std::string(argv[i]) == "--previous") {
        const auto older_ta = read<std::uint32_t>(argv[i + 1]);
        dream::render::DisplayList older;
        older.feed_stream(older_ta.data(), older_ta.size());
        dream::render::add_background(older.frame(), regs.data(), vram.data(), vram.size());
        prepare(older.frame());
        dream::render::Frame between;
        dream::render::BlendStats stats;
        std::vector<dream::render::StripPair> pairs;
        dream::render::BlendOptions blend;
        for (int j=4;j+1<argc;++j) if (std::string(argv[j])=="--blend-t") {
            try { blend.t=std::stof(argv[j+1]); } catch (...) { return 2; }
            if (!std::isfinite(blend.t) || blend.t<0 || blend.t>1) return 2;
        }
        if (dream::render::blend_frames(older.frame(), frame, blend, pairs, between, stats)) {
            // Shared terrain vertices must remain coincident in an interpolated frame.
            std::map<std::array<std::uint32_t, 4>, std::array<float, 4>> groups;
            unsigned split = 0;
            float worst = 0;
            for (const auto& list : frame.lists) for (const auto& p : list) {
                if (p.count != 3) continue;
                if (frame.vertices[p.first].z == frame.vertices[p.first + 1].z &&
                    frame.vertices[p.first].z == frame.vertices[p.first + 2].z) continue;
                for (unsigned j = 0; j < p.count; ++j) {
                    const auto& v = frame.vertices[p.first + j];
                    if (v.y < 150 || v.z > 10) continue;
                    const auto& b = between.vertices[p.first + j];
                    const std::array<std::uint32_t, 4> key{p.tcw, std::bit_cast<std::uint32_t>(v.x),
                        std::bit_cast<std::uint32_t>(v.y), std::bit_cast<std::uint32_t>(v.z)};
                    auto [it, fresh] = groups.emplace(key, std::array{b.x, b.y, b.x, b.y});
                    if (!fresh) {
                        const float distance = std::hypot(b.x - it->second[0], b.y - it->second[1]);
                        if (distance > 0.01f) ++split;
                        worst = std::max(worst, distance);
                    }
                }
            }
            std::printf("interpolation: %u split shared vertices, worst %.3f guest pixels; %u blended / %u skipped strips\n",
                        split, worst, stats.blended_strips, stats.jumped_strips);
            frame = std::move(between);
        } else std::printf("interpolation: camera cut, native frame retained\n");
    }
    const unsigned height = wide ? 720 : 960;
    dream::render::vk::Context context;
    if (!context.create({}, false)) { std::fprintf(stderr, "%s\n", context.error().c_str()); return 1; }
    dream::render::vk::Offscreen offscreen;
    if (!offscreen.create(context, 1280, height)) return 1;
    dream::render::vk::Renderer renderer;
    if (!renderer.create(context, offscreen.render_pass())) return 1;
    renderer.set_memory(vram.data(), vram.size(), regs.data() + 0x1000 / 4,
                        static_cast<dream::render::PaletteFormat>(regs[0x108 / 4] & 3));
    renderer.set_text_control(regs[0xe4 / 4]);
    dream::render::vk::FrameGeometry geometry;
    geometry.alpha_ref = static_cast<float>(regs[0x11c / 4] & 255) / 255;
    geometry.shadow_scale = static_cast<float>(regs[0x74 / 4] & 255) / 256;
    geometry.autosort = (regs[0x7c / 4] & (1u << 21)) != 0 || (regs[0x98 / 4] & 1u) == 0;
    dream::render::describe_fog(regs.data(), geometry.fog);
    if (!offscreen.render(renderer, frame, geometry)) return 1;
    const auto* pixels = offscreen.pixels();
    return pixels && dream::render::png::write_file(argv[3], pixels, 1280, height) ? 0 : 1;
}
