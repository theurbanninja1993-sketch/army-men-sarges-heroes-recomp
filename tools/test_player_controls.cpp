// Real port hooks over synthetic player cameras; no disc data or physical controllers.
#include "dream/runtime/system.h"
#include "dream/runtime/host_ext.h"
#include "dream/render/input.h"
#include "dreamcomp/port.h"
#include "dreamcomp/settings.h"
#include "dreamcomp/controller_mode.h"
#include "dreamcomp/hook.h"
#include "../src/game_layout.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <vector>

namespace dreamcomp { Settings test_settings; Settings& settings() { return test_settings; } }
namespace dream::gen { bool dream_hook_entry_camera_update(dream::sh4::Ctx&, dream::Memory&); }
void army_mouse_settings_changed(dreamcomp::Settings&);
static void check(bool yes, const char* message) {
    if (!yes) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); }
}
int main(int argc, char** argv) {
    check(argc == 2, "pass scratch settings path");
    auto& settings = dreamcomp::test_settings;
    settings.load(argv[1]);
    dream::System sys;
    auto& m = sys.memory;
    dream::host::Extension* extension = nullptr;
    for (auto* e : dream::host::extensions())
        if (std::string(e->name()) == "Army Men mouse camera") extension = e;
    check(extension != nullptr,"camera extension registered");
    constexpr std::uint32_t first = 0x8c300000, actor = 0x8c400018;
    auto camera = [&](unsigned p) { return first + p * army::layout::player_camera_stride; };
    auto angles = [&](unsigned p) { return actor + p * 0x400; };
    auto init = [&] {
        m.write32(army::layout::mission_phase,3);
        m.write32(army::layout::mission_phase+16,first);
        m.write8(army::layout::menu_inhibit,0);
        for (unsigned p=0;p<4;++p) {
            m.write32(camera(p)+0x64,1);
            m.write32(camera(p)+0xdc,army::layout::follow_camera);
            m.write32(camera(p)+0xa4,angles(p));
            dreamcomp::write_f32(m, camera(p)+0x10,10.f*(p+1));
            dreamcomp::write_f32(m, camera(p)+0xa8,10.f);
            dreamcomp::write_f32(m, angles(p)+4,10.f*(p+1));
        }
        extension->on_start(sys);
    };
    const std::array<std::array<bool,4>,4> modes{{
        {{false,false,false,false}}, {{true,true,true,true}},
        {{true,false,true,true}}, {{false,true,false,false}}
    }};
    for (unsigned scenario=0;scenario<modes.size();++scenario) {
        settings.set("controller_mode",scenario==1 || scenario==2 ? "modern":"classic");
        for (unsigned p=0;p<4;++p) settings.set(dreamcomp::controller_mode_key(p),"inherit");
        if (scenario==2) { settings.set("controller_mode_p2","classic");settings.set("controller_mode_p4","modern"); }
        if (scenario==3) settings.set("controller_mode_p2","modern");
        init();
        for (unsigned p=0;p<4;++p) extension->on_controller_camera_player(p,.5f,.25f,true);
        extension->on_vblank(sys);
        for (unsigned p : {3u,1u,0u,2u}) {
            dream::render::GamepadSample physical,logical;
            physical.controls[unsigned(dream::render::PadControl::RightTrigger)]=1;
            check(extension->map_controller_player(p,physical,logical)==modes[scenario][p],"each player's contextual map uses resolved mode");
            if (modes[scenario][p]) check(logical.controls[unsigned(dream::render::PadControl::X)]==1,"modern RT fires for every player");
            sys.ctx.r[4]=camera(p);
            dream::gen::dream_hook_entry_camera_update(sys.ctx,m);
            const auto yaw=dreamcomp::read_f32(m,camera(p)+0x10);
            check(std::abs(yaw-(10.f*(p+1)+(modes[scenario][p] ? .5f*180.f/59.94f:0.f)))<.001f,"only the owning modern camera turns");
            check(std::abs(yaw-dreamcomp::read_f32(m,angles(p)+4))<.001f,"actor heading stays with its camera");
            dream::gen::dream_hook_entry_camera_update(sys.ctx,m);
            check(dreamcomp::read_f32(m,camera(p)+0x10)==yaw,"second camera call cannot reuse motion");
        }
    }
    // Mode changes, per-player release, and pause cannot leak another player's input.
    settings.set("controller_mode","modern");
    for(unsigned p=0;p<4;++p)settings.set(dreamcomp::controller_mode_key(p),"inherit");
    init();
    for(unsigned p=0;p<4;++p)extension->on_controller_camera_player(p,1,1,true);
    extension->on_controller_camera_player(1,0,0,false);
    extension->on_vblank(sys);
    sys.ctx.r[4]=camera(1);dream::gen::dream_hook_entry_camera_update(sys.ctx,m);
    check(dreamcomp::read_f32(m,camera(1)+0x10)==20,"disconnect clears only that player's motion");
    sys.ctx.r[4]=camera(2);dream::gen::dream_hook_entry_camera_update(sys.ctx,m);
    check(dreamcomp::read_f32(m,camera(2)+0x10)>30,"another player's motion survives disconnect");
    settings.set("controller_mode_p1","classic");army_mouse_settings_changed(settings);
    sys.ctx.r[4]=camera(0);dream::gen::dream_hook_entry_camera_update(sys.ctx,m);
    check(dreamcomp::read_f32(m,camera(0)+0x10)==10,"applying Classic clears accumulated motion");
    m.write8(army::layout::menu_inhibit,1);extension->on_vblank(sys);
    sys.ctx.r[4]=camera(3);dream::gen::dream_hook_entry_camera_update(sys.ctx,m);
    check(dreamcomp::read_f32(m,camera(3)+0x10)==40,"pause blocks every player's camera");
    // Persistence and fallback are exercised through the actual settings parser.
    check(settings.save(),"save override settings");
    dreamcomp::Settings loaded;loaded.load(argv[1]);
    check(!dreamcomp::controller_is_modern(loaded,0) && dreamcomp::controller_is_modern(loaded,3),"saved per-player overrides and inheritance reload");
    loaded.set("controller_mode","classic");
    check(!dreamcomp::controller_is_modern(loaded,3),"inherited mode follows main setting changes");
    loaded.set("controller_mode_p4","invalid");
    check(dreamcomp::controller_mode_override(loaded,3)=="inherit","unknown mode safely inherits main setting");
    std::puts("PASS: four-player inheritance, mappings, camera isolation, live changes, pause, release and persistence");
}
