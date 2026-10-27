// Uses local RAM snapshots as fixtures; never writes to the user's save or disc.
#undef NDEBUG
#include "dream/runtime/host_ext.h"
#include "dream/runtime/system.h"
#include "dream/render/input.h"
#include <SDL3/SDL.h>
#include <cassert>
#include <cstdio>
#include <fstream>
#include <string>
using dream::render::PadControl;
static void load(dream::System& sys, const std::string& path) {
    std::ifstream f(path,std::ios::binary);f.read(reinterpret_cast<char*>(sys.memory.ram()),16*1024*1024);
    assert(f.gcount()==16*1024*1024);
}
int main(int argc,char**argv) {
    if(argc!=5) { std::fprintf(stderr,"usage: %s main.ram code.ram pause.ram startup.ram\n",argv[0]);return 2; }
    auto* menu=dream::host::extensions().back();
    dream::System sys;
    bool released=false;
    auto& hc=dream::host::host_controls();
    hc.set_pointer_input=[&](bool on){released=on;};
    hc.window_aspect=[] {return 16.f/9.f;};
    hc.normalize_pointer=[](float x,float y,float& nx,float&ny){nx=x/1280;ny=y/720;return true;};
    auto key=[&](SDL_Keycode k){SDL_Event e{};e.type=SDL_EVENT_KEY_DOWN;e.key.key=k;return menu->on_event(&e);};
    auto click=[&](float x,float y,unsigned button=SDL_BUTTON_LEFT){
        SDL_Event e{};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=button;
        e.button.x=(.5f+(x/320-.5f)*.75f)*1280;e.button.y=y/240*720;
        assert(menu->on_event(&e));menu->on_vblank(sys);
    };
    auto value=[&](PadControl c){dream::render::KeyboardSample k;dream::render::GamepadSample p;
        assert(menu->map_keyboard(k,p));return p.controls[unsigned(c)];};
    load(sys,argv[1]);menu->on_start(sys);menu->on_vblank(sys);assert(released);
    click(200,98);assert(sys.memory.read32(0x8c1a4de0)==2);assert(value(PadControl::A)==1);
    // Black side bars must not select/activate an option in widescreen.
    for(int i=0;i<5;++i)value(PadControl::A);
    SDL_Event e{};e.type=SDL_EVENT_MOUSE_BUTTON_DOWN;e.button.button=SDL_BUTTON_LEFT;e.button.x=0;e.button.y=294;
    assert(menu->on_event(&e));menu->on_vblank(sys);assert(value(PadControl::A)==0);
    load(sys,argv[2]);menu->on_vblank(sys);
    for(auto k:std::string("blzzrx"))assert(key(static_cast<SDL_Keycode>(k)));
    assert(key(SDLK_BACKSPACE));assert(key(static_cast<SDL_Keycode>('D')));menu->on_vblank(sys);
    std::string text;for(unsigned i=0;i<sys.memory.read32(0x8c1a5350);++i)text+=char(sys.memory.read8(0x8c1a5338+i));
    assert(text=="BLZZRD");
    // Gameplay bindings associated with typed keys must not reach native menus.
    dream::render::KeyboardSample raw;raw.controls.fill(1);dream::render::GamepadSample logical;
    assert(menu->map_keyboard(raw,logical));for(auto v:logical.controls)assert(v==0);
    assert(key(SDLK_RETURN));menu->on_vblank(sys);
    assert(sys.memory.read32(0x8c1a5358)==2 && sys.memory.read32(0x8c1a5360)==1);
    assert(value(PadControl::A)==1);
    assert(key(SDLK_DELETE));menu->on_vblank(sys);assert(sys.memory.read32(0x8c1a5350)==0);
    for(auto k:std::string(20,'b'))key(static_cast<SDL_Keycode>(k));menu->on_vblank(sys);
    assert(sys.memory.read32(0x8c1a5350)==12 && sys.memory.read8(0x8c1a5338+12)==0);
    load(sys,argv[3]);menu->on_vblank(sys);assert(released);
    click(160,115);const auto player=sys.memory.read32(0x8c18cc70);
    const auto panel=sys.memory.read32(0x8c0cfda0+4*player);assert(sys.memory.read32(panel+8)==1);
    click(160,95,SDL_BUTTON_RIGHT);
    // A transition to gameplay discards all queued menu actions and capture requests.
    sys.memory.write32(panel,0);menu->on_vblank(sys);assert(!released);
    assert(!menu->map_keyboard(raw,logical));
    load(sys,argv[4]);menu->on_vblank(sys);assert(released);
    click(78,97);assert(value(PadControl::A)==1);
    for(int i=0;i<5;++i)value(PadControl::A);
    click(200,97);assert(value(PadControl::B)==1);
    // Initial startup uses phase 0; returning frontend paths also use phase 7.
    sys.memory.write32(0x8c18ed1c,0);sys.memory.write32(0x8c0eb1d0,5);
    sys.memory.write32(0x8c1a53c0,0);menu->on_vblank(sys);assert(released);
    click(200,98);assert(sys.memory.read32(0x8c1a4de0)==2);assert(value(PadControl::A)==1);
    // The title's Start prompt also accepts a mouse click.
    sys.memory.write32(0x8c0eb1d0,4);menu->on_vblank(sys);
    click(160,180);assert(value(PadControl::Start)==1);
    std::puts("PASS: native SDL pointer mapping, password typing/editing, bounds and gameplay isolation");
}
