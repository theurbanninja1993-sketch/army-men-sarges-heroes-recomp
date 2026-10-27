#include "dreamcomp/hook.h"
#include "dreamcomp/port.h"
#include "dreamcomp/settings.h"
#include "dream/runtime/host_ext.h"
#include "dream/runtime/system.h"
#include "widescreen.h"
#include "controller_controls.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <utility>

namespace {
float pending_x = 0, pending_y = 0;
float sensitivity = 0.12f, aim_sensitivity = 0.12f;
bool modern_controller = false;
bool gameplay_camera = false, gameplay_input = false, native_menu_blocked = false;
std::uint64_t gameplay_camera_frame = 0;
float stick_x = 0, stick_y = 0;
float pending_stick_x = 0, pending_stick_y = 0;
std::uint64_t stick_consumed_frame = 0;
bool enabled = true, invert_y = false;
bool vertical = true, reset_pitch = false;
std::uint64_t frame = 0;
std::uint64_t pending_frame = 0;

float wrap(float degrees) {
    degrees = std::fmod(degrees, 360.0f);
    return degrees < 0 ? degrees + 360.0f : degrees;
}
float signed_angle(float degrees) {
    return std::remainder(degrees, 360.0f);
}

void refresh_settings(dreamcomp::Settings& settings) {
    const bool modern = settings.get("controller_mode", "classic") == "modern";
    if (modern != modern_controller) {
        stick_x = stick_y = pending_stick_x = pending_stick_y = 0;
    }
    modern_controller = modern;
    enabled = settings.get_bool("mouse_look", true);
    invert_y = settings.get_bool("mouse_invert_y", false);
    const bool next_vertical = settings.get_bool("mouse_vertical", true);
    if (vertical && !next_vertical) reset_pitch = true;
    vertical = next_vertical;
    sensitivity = settings.get_float("mouse_sensitivity", 0.12f);
    if (!std::isfinite(sensitivity)) sensitivity = 0.12f;
    sensitivity = std::clamp(sensitivity, 0.001f, 5.0f);
    // Migrate existing settings once, preserving the old feel in both modes.
    if (settings.get("mouse_aim_sensitivity").empty())
        settings.set("mouse_aim_sensitivity", std::to_string(sensitivity));
    aim_sensitivity = settings.get_float("mouse_aim_sensitivity", sensitivity);
    if (!std::isfinite(aim_sensitivity)) aim_sensitivity = sensitivity;
    aim_sensitivity = std::clamp(aim_sensitivity, 0.001f, 5.0f);
}

struct MouseCamera final : dream::host::Extension {
    const char* name() const override { return "Army Men mouse camera"; }
    bool wants_mouse_motion() override { return true; }
    bool wants_keyboard_strafe() override { return true; }
    bool controller_camera_enabled() override { return modern_controller && !native_menu_blocked; }
    bool map_keyboard(const dream::render::KeyboardSample& physical,
                      dream::render::GamepadSample& logical) override {
        if (!physical.aiming || native_menu_blocked || !gameplay_input)
            return false;
        logical = army_aim_keyboard(physical);
        return true;
    }
    bool map_controller(const dream::render::GamepadSample& physical,
                        dream::render::GamepadSample& logical) override {
        // Leave game menus/cinematics on their stock bindings, even in Modern.
        if (!modern_controller || native_menu_blocked || !gameplay_input)
            return false;
        logical = army_modern_controls(physical);
        return true;
    }
    void on_controller_camera(float x, float y, bool active) override {
        stick_x = active && modern_controller && std::isfinite(x) ? std::clamp(x, -1.0f, 1.0f) : 0;
        stick_y = active && modern_controller && std::isfinite(y) ? std::clamp(y, -1.0f, 1.0f) : 0;
        if (!active) pending_stick_x = pending_stick_y = 0;
    }
    void on_mouse_motion(float dx, float dy, bool active) override {
        if (!active) {
            pending_x = pending_y = 0;
            return;
        }
        pending_x += dx;
        pending_y += dy;
        if (dx != 0 || dy != 0) pending_frame = frame;
    }
    void on_stop(dream::System&, const char*) override {
        if (std::getenv("ARMY_TRACE_CONTROLLER"))
            std::printf("controller context: frame=%llu last_gameplay_camera=%llu gameplay=%u menu_blocked=%u\n",
                        static_cast<unsigned long long>(frame),
                        static_cast<unsigned long long>(gameplay_camera_frame), gameplay_camera, native_menu_blocked);
    }
    void on_start(dream::System&) override {
        gameplay_input = false;
        refresh_settings(dreamcomp::settings());
        pending_x = pending_y = stick_x = stick_y = pending_stick_x = pending_stick_y = 0;
    }
    void on_vblank(dream::System& sys) override {
        frame = sys.spg.frames();
        // Input profiles follow mission/menu state, independently of camera cadence.
        // Reverting to stock trigger ramping on a skipped camera tick drops held aim.
        gameplay_input = sys.memory.read32(0x8c18ed1cu) == 3;
        // fn_0c0364ba inhibits gameplay input while native menus are active,
        // including a short release debounce. The camera still updates while paused.
        native_menu_blocked = sys.memory.read8(0x8c18e770u) != 0;
        if (native_menu_blocked) {
            stick_x = stick_y = pending_stick_x = pending_stick_y = 0;
        }
        // Test the native camera without a physical pad; outside the interval the
        // stick is released, just as a live inactive sample releases it.
        if (const char* test = std::getenv("ARMY_TEST_STICK")) {
            unsigned long long from = 0, to = 0;
            float x = 0, y = 0;
            if (std::sscanf(test, "%llu:%llu:%f:%f", &from, &to, &x, &y) == 4)
                on_controller_camera(x, y, frame >= from && frame < to);
        }
        // Integrate at the guest's 59.94 Hz, independently of host refresh and
        // internal resolution. Keep both samples for a 30 Hz camera update.
        if (frame - stick_consumed_frame > 2) pending_stick_x = pending_stick_y = 0;
        if (modern_controller) {
            pending_stick_x += stick_x * (180.0f / 59.94f);
            pending_stick_y += stick_y * (90.0f / 59.94f);
        }
        // Gameplay updates at 30 Hz while input is sampled at 60 Hz. Keep both samples
        // until the next camera update, but do not carry motion across a cinematic.
        if (frame - pending_frame > 2) pending_x = pending_y = 0;
        // Reproduce a physical mouse sample without a window, once at the specified guest frame.
        if (const char* test = std::getenv("ARMY_TEST_MOUSE")) {
            unsigned long long at = 0;
            float x = 0, y = 0;
            if (std::sscanf(test, "%llu:%f:%f", &at, &x, &y) == 3 && frame == at &&
                std::isfinite(x) && std::isfinite(y)) {
                pending_x = x;
                pending_y = y;
                pending_frame = frame;
            }
        }
    }
};
MouseCamera mouse_camera;
dream::host::Register register_mouse_camera(mouse_camera);
}  // namespace

void army_mouse_settings_changed(dreamcomp::Settings& settings) {
    refresh_settings(settings);
}

DC_HOOK_ENTRY(camera_update) {
    army_camera_aspect(m, c.r[4]);
    if (native_menu_blocked) {
        pending_x = pending_y = pending_stick_x = pending_stick_y = 0;
        return false;
    }
    const auto camera = c.r[4];
    const auto callback = m.read32(camera + 0xdc) & 0x1fffffffu;
    const auto mode = m.read32(camera + 0x64);
    // Only the campaign follow camera and first-person targeting camera; cinematics use others.
    gameplay_camera = (mode == 1 && callback == 0x0c057964) ||
                      (mode == 4 && callback == 0x0c057da8);
    if (!gameplay_camera) {
        pending_stick_x = pending_stick_y = 0;
        stick_consumed_frame = frame;
        return false;
    }
    gameplay_camera_frame = frame;
    if ((!enabled || (pending_x == 0 && pending_y == 0 && !reset_pitch)) &&
        pending_stick_x == 0 && pending_stick_y == 0) return false;
    if (reset_pitch) {
        dreamcomp::write_f32(m, camera + (mode == 4 ? 0xc : 0xa8), mode == 4 ? 0.0f : 10.0f);
        reset_pitch = false;
    }
    const float dx = enabled ? std::exchange(pending_x, 0.0f) : 0;
    const float dy = enabled ? std::exchange(pending_y, 0.0f) : 0;
    const float stick_yaw = std::exchange(pending_stick_x, 0.0f);
    const float stick_pitch = std::exchange(pending_stick_y, 0.0f);
    stick_consumed_frame = frame;
    const auto angles = m.read32(camera + 0xa4);
    const auto physical = angles & 0x1fffffffu;
    if (physical < 0x0c000000 || physical > 0x0cffff9e) return false;

    const float yaw = dreamcomp::read_f32(m, angles + 4);
    const float camera_yaw = dreamcomp::read_f32(m, camera + 0x10);
    if (!std::isfinite(yaw) || !std::isfinite(camera_yaw)) return false;
    const float mouse_speed = mode == 4 ? aim_sensitivity : sensitivity;
    const float yaw_delta = dx * mouse_speed + stick_yaw;
    const float pitch_delta = (vertical ? dy * mouse_speed * (invert_y ? -1.0f : 1.0f) : 0.0f) + stick_pitch;
    // Keep the player's firing direction and the camera together. Applying the same yaw step
    // to both preserves the follow camera's existing lag without smoothing the mouse sample.
    const float next_yaw = wrap(yaw + yaw_delta);
    dreamcomp::write_f32(m, angles + 4, next_yaw);
    // Actor +0x78 is the authoritative 16-bit heading. The float at +0x1c is
    // regenerated from it by fn_0c0418dc, so changing the float alone lasts one frame.
    m.write16(angles + 0x60, static_cast<std::uint16_t>(
        static_cast<unsigned>(std::lround(next_yaw * (65536.0f / 360.0f))) & 0xffffu));
    if (mode == 4) {
        const float camera_pitch = signed_angle(dreamcomp::read_f32(m, camera + 0xc));
        dreamcomp::write_f32(m, camera + 0xc, std::clamp(camera_pitch + pitch_delta, -75.0f, 75.0f));
    } else {
        // Follow mode uses this pitch offset both for its orbit and its smoothing target.
        // Changing the actor's pitch instead makes those two calculations disagree.
        const float offset = signed_angle(dreamcomp::read_f32(m, camera + 0xa8));
        dreamcomp::write_f32(m, camera + 0xa8, std::clamp(offset + pitch_delta, -65.0f, 75.0f));
    }
    dreamcomp::write_f32(m, camera + 0x10, wrap(camera_yaw + yaw_delta));
    if (std::getenv("ARMY_TRACE_CAMERA"))
        std::printf("mouse camera frame=%llu mode=%u dx=%g dy=%g yaw=%g pitch=%g\n",
                    static_cast<unsigned long long>(frame), mode, dx, dy,
                    dreamcomp::read_f32(m, angles + 4), dreamcomp::read_f32(m, camera + 0xc));
    return false;
}
