// Army Men: Sarge's Heroes: what this port tells dreamcomp. Game facts with sources: docs/GAME-INTERNALS.md.
#include "dreamcomp/hook.h"
#include "dreamcomp/port.h"
#include "widescreen.h"

void army_mouse_settings_changed(dreamcomp::Settings& settings);

namespace {

const dreamcomp::PortInfo kInfo = [] {
    dreamcomp::PortInfo p;
    p.id = "armymensh";
    p.title = "Army Men: Sarge's Heroes";
    p.mouse_camera = true;
    p.controller_camera = true;
    p.controller_help = "Classic keeps the original controller mappings. Modern: right trigger fires, "
                        "X changes weapon, B goes prone, and A jumps. Left trigger aims; while aiming, "
                        "D-pad up/down zoom and right bumper quick-zooms. Outside aim, hold right bumper "
                        "to enable left-stick strafing. Right stick controls the camera.";
    p.on_settings_changed = army_mouse_settings_changed;
    p.widescreen = army_set_aspect;
    p.max_aspect = 16.0f / 9.0f;
    p.pillarbox_2d = true;
    p.hud.enabled = true;
    p.hud.polygons = true;
    return p;
}();
dreamcomp::RegisterPort g_register(kInfo);

}  // namespace
