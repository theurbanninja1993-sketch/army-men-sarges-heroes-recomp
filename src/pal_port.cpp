// Experimental English60 port features, enabled only after their PAL layouts are measured.
#include "dreamcomp/port.h"
#include "dreamcomp/settings.h"
#include "widescreen.h"

void army_mouse_settings_changed(dreamcomp::Settings&);
namespace {
const dreamcomp::PortInfo info = [] {
    dreamcomp::PortInfo p;
    p.id = "armymensh_pal_en60";
    p.title = "Army Men: Sarge's Heroes (PAL English 60 Hz preview)";
    p.mouse_camera = true;
    p.controller_camera = true;
    p.widescreen = army_set_aspect;
    p.max_aspect = 16.0f / 9.0f;
    p.pillarbox_2d = true;
    p.hud.enabled = true;
    p.hud.polygons = true;
    p.hud_overrides_file = "hud_overrides_pal_en60.ini";
    p.controller_help = "Classic keeps the original mappings. Modern uses the right stick for the "
        "camera, right trigger to fire, X to change weapon, B for prone and left trigger to aim. "
        "Hold right bumper for strafing; while aiming, D-pad up/down zoom and right bumper quick-zooms.";
    p.on_settings_changed = army_mouse_settings_changed;
    return p;
}();
dreamcomp::RegisterPort registration(info);
}
