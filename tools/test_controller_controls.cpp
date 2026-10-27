// Pure mapping checks, plus optional real SDL virtual-controller checks (--window).
#include "../src/controller_controls.h"
#include "dream/render/vk/window.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <string>
using namespace dream::render;
static void check(bool condition, const char* message) {
    if (!condition) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); }
}
static unsigned index(PadControl c) { return static_cast<unsigned>(c); }
static float value(const GamepadSample& s, PadControl c) { return s.controls[index(c)]; }
int main(int argc, char** argv) {
    GamepadSample p;
    p.controls[index(PadControl::RightTrigger)] = 1;
    auto result = army_modern_controls(p);
    check(value(result,PadControl::X)==1 && value(result,PadControl::RightTrigger)==0,"RT fires without quick zoom/strafe");
    p={}; p.controls[index(PadControl::X)]=1;
    result=army_modern_controls(p);
    check(value(result,PadControl::B)==1 && value(result,PadControl::X)==0,"X changes weapon, does not fire");
    p={}; p.controls[index(PadControl::B)]=1;
    result=army_modern_controls(p);
    check(value(result,PadControl::Y)==1 && value(result,PadControl::B)==0,"B goes prone, does not change weapon");
    p={}; p.controls[index(PadControl::StickLeft)]=1;
    result=army_modern_controls(p);
    check(value(result,PadControl::StickLeft)==0,"sideways stick alone is ignored");
    p.right_bumper=true; result=army_modern_controls(p);
    check(value(result,PadControl::StickLeft)==1 && value(result,PadControl::RightTrigger)==1,"RB enables strafe");
    p.controls[index(PadControl::LeftTrigger)]=1;
    p.controls[index(PadControl::Up)]=1; result=army_modern_controls(p);
    check(value(result,PadControl::Y)==1 && value(result,PadControl::Up)==0,"LT+dpad up zooms in");
    check(value(result,PadControl::RightTrigger)==1 && value(result,PadControl::StickLeft)==0,"aim+RB quick zoom without sideways turn");
    p.controls[index(PadControl::Up)]=0; p.controls[index(PadControl::Down)]=1;
    result=army_modern_controls(p);
    check(value(result,PadControl::A)==1 && value(result,PadControl::Down)==0,"LT+dpad down zooms out");
    p={}; p.controls[index(PadControl::Up)]=1; p.controls[index(PadControl::Down)]=1;
    result=army_modern_controls(p);
    check(value(result,PadControl::Up)==1 && value(result,PadControl::Down)==1 && value(result,PadControl::Y)==0,"dpad outside aim stays stock");
    p={}; p.controls[index(PadControl::LeftTrigger)]=1;
    p.controls[index(PadControl::B)]=1; p.controls[index(PadControl::A)]=1;
    result=army_modern_controls(p);
    check(value(result,PadControl::Y)==0 && value(result,PadControl::A)==0,"face buttons do not change zoom during aim");
    KeyboardSample keys; keys.aiming=true; keys.forward=true;
    keys.controls[index(PadControl::StickUp)]=1;
    auto keyboard=army_aim_keyboard(keys);
    check(value(keyboard,PadControl::Y)==1 && value(keyboard,PadControl::StickUp)==0,"RMB+W zooms without movement");
    keys.forward=false; keys.backward=true; keys.shift=true;
    keys.controls[index(PadControl::StickUp)]=0; keys.controls[index(PadControl::StickDown)]=1;
    keyboard=army_aim_keyboard(keys);
    check(value(keyboard,PadControl::A)==1 && value(keyboard,PadControl::StickDown)==0 && value(keyboard,PadControl::RightTrigger)==1,"RMB+S/Shift zoom out/quick zoom");
    if (argc<2 || std::string(argv[1])!="--window") {
        std::puts("PASS: contextual controller and keyboard mappings"); return 0;
    }
    SDL_SetHint(SDL_HINT_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT,"0xfeed/0xbeef");
    vk::Window window;
    check(window.create("Army Men input verification",640,480,false),window.error().c_str());
    // Wayland grants keyboard focus after the first surface commit.
    std::uint32_t image=0; VkCommandBuffer cmd=VK_NULL_HANDLE;
    check(window.begin_frame(image,cmd),"begin initial window frame");
    VkClearValue clear[2]{}; clear[0].color={{0,0,0,1}};
    VkRenderPassBeginInfo pass{};pass.sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    pass.renderPass=window.render_pass();pass.framebuffer=window.framebuffer(image);
    pass.renderArea.extent=window.extent();pass.clearValueCount=2;pass.pClearValues=clear;
    vkCmdBeginRenderPass(cmd,&pass,VK_SUBPASS_CONTENTS_INLINE);vkCmdEndRenderPass(cmd);
    check(window.end_frame(image),"present initial window frame");
    SDL_VirtualJoystickDesc desc; SDL_INIT_INTERFACE(&desc);
    desc.vendor_id=0xfeed;desc.product_id=0xbeef;
    desc.type=SDL_JOYSTICK_TYPE_GAMEPAD; desc.naxes=6; desc.nbuttons=16;
    desc.axis_mask=63; desc.button_mask=65535; desc.name="Army input test";
    const auto id=SDL_AttachVirtualJoystick(&desc);
    auto* joystick=SDL_OpenJoystick(id);
    check(joystick!=nullptr,"open virtual controller");
    bool modern=true, gameplay=true;
    window.controller_camera_enabled=[&] { return modern; };
    window.controller_map=[&](const GamepadSample& raw,GamepadSample& mapped) {
        if (!gameplay) return false;
        mapped=army_modern_controls(raw); return true;
    };
    auto reset=[&] {
        for(int a=0;a<6;++a) SDL_SetJoystickVirtualAxis(joystick,a,a>=4 ? -32768 : 0);
        for(int b=0;b<16;++b) SDL_SetJoystickVirtualButton(joystick,b,false);
    };
    auto poll=[&] { SDL_UpdateJoysticks(); window.poll(); };
    auto v=[&](PadControl c) { return window.pad_value(c); };
    reset();
    for(int n=0;n<20;++n) { poll(); SDL_Delay(5); }
    SDL_SetJoystickVirtualAxis(joystick,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,32767);poll();
    check(v(PadControl::X)==1 && v(PadControl::RightTrigger)==0,"SDL RT fire mapping");
    reset();SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_WEST,true);poll();
    check(v(PadControl::B)==1 && v(PadControl::X)==0,"SDL X weapon mapping");
    reset();SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_EAST,true);poll();
    check(v(PadControl::Y)==1 && v(PadControl::B)==0,"SDL B prone mapping");
    reset();SDL_SetJoystickVirtualAxis(joystick,SDL_GAMEPAD_AXIS_LEFTX,32767);poll();
    check(v(PadControl::StickRight)==0 && v(PadControl::RightTrigger)==0,"SDL no unmodified strafe/turn");
    SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,true);poll();
    check(v(PadControl::StickRight)==1 && v(PadControl::RightTrigger)==1,"SDL RB strafe");
    SDL_SetJoystickVirtualAxis(joystick,SDL_GAMEPAD_AXIS_LEFT_TRIGGER,32767);
    SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_DPAD_UP,true);poll();
    check(v(PadControl::Y)==1 && v(PadControl::Up)==0 && v(PadControl::StickRight)==0 && v(PadControl::RightTrigger)==1,"SDL aim zoom plus RB quick zoom");
    reset();SDL_SetJoystickVirtualAxis(joystick,SDL_GAMEPAD_AXIS_LEFT_TRIGGER,32767);
    SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_DPAD_DOWN,true);poll();
    check(v(PadControl::A)==1 && v(PadControl::Down)==0,"SDL aim zoom out");
    modern=false;reset();SDL_SetJoystickVirtualAxis(joystick,SDL_GAMEPAD_AXIS_RIGHT_TRIGGER,32767);poll();
    check(v(PadControl::RightTrigger)==1 && v(PadControl::X)==0,"SDL Classic RT stays stock");
    modern=true;gameplay=false;reset();SDL_SetJoystickVirtualButton(joystick,SDL_GAMEPAD_BUTTON_EAST,true);poll();
    check(v(PadControl::B)==1 && v(PadControl::Y)==0,"SDL game-menu back stays stock");
    window.set_game_input_blocked(true);poll();
    check(v(PadControl::B)==0,"settings menu blocks game input");
    if (std::getenv("ARMY_INPUT_SHIM")) {
        window.set_game_input_blocked(false);
        window.keyboard_strafe=true;gameplay=true;modern=false;
        window.keyboard_map=[&](const KeyboardSample& raw,GamepadSample& mapped) {
            if (!gameplay || !raw.aiming) return false;
            mapped=army_aim_keyboard(raw);return true;
        };
        SDL_setenv_unsafe("ARMY_INPUT_KEYS","w",1);poll();
        check(v(PadControl::StickUp)==1 && v(PadControl::Y)==0,"SDL W moves outside aim");
        SDL_setenv_unsafe("ARMY_INPUT_KEYS","r,w",1);poll();
        check(v(PadControl::LeftTrigger)==1 && v(PadControl::Y)==1 && v(PadControl::StickUp)==0,"SDL RMB+W zoom without movement");
        SDL_setenv_unsafe("ARMY_INPUT_KEYS","r,s,",1);poll();
        check(v(PadControl::A)==1 && v(PadControl::StickDown)==0,"SDL RMB+S zoom out");
        SDL_setenv_unsafe("ARMY_INPUT_KEYS","r,shift",1);poll();
        check(v(PadControl::RightTrigger)==1,"SDL RMB+Shift immediate quick zoom");
        SDL_setenv_unsafe("ARMY_INPUT_KEYS","r,a",1);poll();
        check(v(PadControl::RightTrigger)==0,"SDL aim sideways key must not quick zoom");
        SDL_setenv_unsafe("ARMY_INPUT_KEYS","shift",1);poll();
        check(v(PadControl::RightTrigger)==0,"SDL Shift outside aim unchanged");
        SDL_setenv_unsafe("ARMY_INPUT_KEYS","w",1);poll();
        check(v(PadControl::StickUp)==1 && v(PadControl::Y)==0,"SDL W movement restored after aim");
    }
    SDL_CloseJoystick(joystick);SDL_DetachVirtualJoystick(id);
    std::puts("PASS: contextual mappings and real SDL controller input");
}
