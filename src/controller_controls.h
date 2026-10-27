#pragma once
#include "dream/render/input.h"

// Modern physical controller -> the game's original Dreamcast actions.
// Keyboard/mouse input is merged separately by the host.
inline dream::render::GamepadSample army_modern_controls(
    const dream::render::GamepadSample& physical) {
    using dream::render::PadControl;
    auto result = physical;
    const auto input = [&](PadControl c) { return physical.controls[static_cast<unsigned>(c)]; };
    const auto set = [&](PadControl c, float value) { result.controls[static_cast<unsigned>(c)] = value; };
    const bool aiming = input(PadControl::LeftTrigger) > 0;
    set(PadControl::X, input(PadControl::RightTrigger));  // fire
    set(PadControl::B, input(PadControl::X));             // change weapon
    set(PadControl::Y, aiming ? input(PadControl::Up) : input(PadControl::B));
    set(PadControl::A, aiming ? input(PadControl::Down) : input(PadControl::A));
    set(PadControl::RightTrigger, physical.right_bumper ? 1.0f : 0.0f);
    if (aiming) {
        set(PadControl::Up, 0);
        set(PadControl::Down, 0);
    }
    // Right bumper is the strafe modifier outside aim, and quick zoom during aim.
    // Never feed an unintended left-stick turn into either camera mode.
    if (!physical.right_bumper || aiming) {
        set(PadControl::StickLeft, 0);
        set(PadControl::StickRight, 0);
    }
    return result;
}

inline dream::render::GamepadSample army_aim_keyboard(
    const dream::render::KeyboardSample& physical) {
    using dream::render::PadControl;
    dream::render::GamepadSample result;
    result.controls = physical.controls;
    auto& controls = result.controls;
    if (physical.forward) {
        controls[static_cast<unsigned>(PadControl::StickUp)] = 0;
        controls[static_cast<unsigned>(PadControl::Y)] = 1;  // zoom in
    }
    if (physical.backward) {
        controls[static_cast<unsigned>(PadControl::StickDown)] = 0;
        controls[static_cast<unsigned>(PadControl::A)] = 1;  // zoom out
    }
    controls[static_cast<unsigned>(PadControl::RightTrigger)] = physical.shift ? 1.0f : 0.0f;
    return result;
}
