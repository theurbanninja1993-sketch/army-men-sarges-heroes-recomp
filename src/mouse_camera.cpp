#include "dreamcomp/hook.h"
#include "dreamcomp/port.h"
#include "dreamcomp/settings.h"
#include "dreamcomp/controller_mode.h"
#include "dream/runtime/host_ext.h"
#include "dream/runtime/system.h"
#include "widescreen.h"
#include "controller_controls.h"
#include "game_layout.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <utility>

namespace {
float pending_x = 0, pending_y = 0;
float sensitivity = 0.12f, aim_sensitivity = 0.12f;
struct ControllerInput {
    bool modern = false;
    float stick_x = 0, stick_y = 0, pending_x = 0, pending_y = 0;
    std::uint64_t consumed_frame = 0;
};
std::array<ControllerInput, 4> controllers;
void clear_stick(ControllerInput& input) {
    input.stick_x = input.stick_y = input.pending_x = input.pending_y = 0;
}
int camera_player(dream::Memory& memory, std::uint32_t camera) {
    const auto first = memory.read32(army::layout::mission_phase + 16) & 0x1fffffffu;
    const auto address = camera & 0x1fffffffu;
    if (first < 0x0c000000u || first > 0x0cfff000u || address < first) return -1;
    const auto offset = address - first;
    if (offset % army::layout::player_camera_stride != 0) return -1;
    const auto player = offset / army::layout::player_camera_stride;
    return player < controllers.size() ? static_cast<int>(player) : -1;
}
bool gameplay_camera = false, gameplay_input = false, native_menu_blocked = false;
std::uint64_t gameplay_camera_frame = 0;
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
    for (unsigned player = 0; player < controllers.size(); ++player) {
        auto& input = controllers[player];
        const bool modern = dreamcomp::controller_is_modern(settings, player);
        if (modern != input.modern) clear_stick(input);
        input.modern = modern;
    }
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
    bool controller_camera_enabled() override { return controller_camera_enabled_player(0); }
    bool controller_camera_enabled_player(unsigned player) override {
        return player < controllers.size() && controllers[player].modern && !native_menu_blocked;
    }
    bool map_keyboard(const dream::render::KeyboardSample& physical,
                      dream::render::GamepadSample& logical) override {
        if (!physical.aiming || native_menu_blocked || !gameplay_input)
            return false;
        logical = army_aim_keyboard(physical);
        return true;
    }
    bool map_controller(const dream::render::GamepadSample& physical,
                        dream::render::GamepadSample& logical) override {
        return map_controller_player(0, physical, logical);
    }
    bool map_controller_player(unsigned player, const dream::render::GamepadSample& physical,
                               dream::render::GamepadSample& logical) override {
        if (!controller_camera_enabled_player(player) || !gameplay_input) return false;
        logical = army_modern_controls(physical);
        return true;
    }
    void on_controller_camera(float x, float y, bool active) override {
        on_controller_camera_player(0, x, y, active);
    }
    void on_controller_camera_player(unsigned player, float x, float y, bool active) override {
        if (player >= controllers.size()) return;
        auto& input = controllers[player];
        active = active && input.modern;
        input.stick_x = active && std::isfinite(x) ? std::clamp(x, -1.0f, 1.0f) : 0;
        input.stick_y = active && std::isfinite(y) ? std::clamp(y, -1.0f, 1.0f) : 0;
        if (!active) input.pending_x = input.pending_y = 0;
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
        controllers = {};
        refresh_settings(dreamcomp::settings());
        pending_x = pending_y = 0;
    }
    void on_vblank(dream::System& sys) override {
        frame = sys.spg.frames();
        // Input profiles follow mission/menu state, independently of camera cadence.
        // Reverting to stock trigger ramping on a skipped camera tick drops held aim.
        gameplay_input = sys.memory.read32(army::layout::mission_phase) == 3;
        // fn_0c0364ba inhibits gameplay input while native menus are active,
        // including a short release debounce. The camera still updates while paused.
        native_menu_blocked = sys.memory.read8(army::layout::menu_inhibit) != 0;
        for (unsigned player = 0; player < controllers.size(); ++player) {
            auto& input = controllers[player];
            // Legacy diagnostic remains player one; numbered variants exercise other players.
            const auto key = "ARMY_TEST_STICK_P" + std::to_string(player + 1);
            const char* test = std::getenv(key.c_str());
            if (!test && player == 0) test = std::getenv("ARMY_TEST_STICK");
            if (test) {
                unsigned long long from = 0, to = 0;
                float x = 0, y = 0;
                if (std::sscanf(test, "%llu:%llu:%f:%f", &from, &to, &x, &y) == 4)
                    on_controller_camera_player(player, x, y, frame >= from && frame < to);
            }
            if (native_menu_blocked || !gameplay_input) {
                clear_stick(input);
                continue;
            }
            if (frame - input.consumed_frame > 2) input.pending_x = input.pending_y = 0;
            if (input.modern) {
                input.pending_x += input.stick_x * (180.0f / 59.94f);
                input.pending_y += input.stick_y * (90.0f / 59.94f);
            }
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
    const auto camera = c.r[4];
    const int player = camera_player(m, camera);
    if (player < 0) return false;
    auto& input = controllers[static_cast<unsigned>(player)];
    if (native_menu_blocked) {
        if (player == 0) pending_x = pending_y = 0;
        clear_stick(input);
        return false;
    }
    const auto callback = m.read32(camera + 0xdc) & 0x1fffffffu;
    const auto mode = m.read32(camera + 0x64);
    const bool valid_camera = (mode == 1 && callback == army::layout::follow_camera) ||
                              (mode == 4 && callback == army::layout::aim_camera);
    if (player == 0) {
        gameplay_camera = valid_camera;
        if (valid_camera) gameplay_camera_frame = frame;
    }
    if (!valid_camera) {
        input.pending_x = input.pending_y = 0;
        input.consumed_frame = frame;
        return false;
    }
    const bool mouse_player = player == 0;
    if ((!mouse_player || !enabled || (pending_x == 0 && pending_y == 0 && !reset_pitch)) &&
        input.pending_x == 0 && input.pending_y == 0) return false;
    if (mouse_player && reset_pitch) {
        dreamcomp::write_f32(m, camera + (mode == 4 ? 0xc : 0xa8), mode == 4 ? 0.0f : 10.0f);
        reset_pitch = false;
    }
    const float dx = mouse_player && enabled ? std::exchange(pending_x, 0.0f) : 0;
    const float dy = mouse_player && enabled ? std::exchange(pending_y, 0.0f) : 0;
    const float stick_yaw = std::exchange(input.pending_x, 0.0f);
    const float stick_pitch = std::exchange(input.pending_y, 0.0f);
    input.consumed_frame = frame;
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
        std::printf("mouse camera frame=%llu player=%u mode=%u dx=%g dy=%g yaw=%g pitch=%g\n",
                    static_cast<unsigned long long>(frame), static_cast<unsigned>(player + 1), mode, dx, dy,
                    dreamcomp::read_f32(m, angles + 4), dreamcomp::read_f32(m, camera + 0xc));
    return false;
}
