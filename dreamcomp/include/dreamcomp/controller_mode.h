#pragma once
#include "dreamcomp/settings.h"

namespace dreamcomp {
inline std::string controller_mode_key(unsigned player) {
    return "controller_mode_p" + std::to_string(player + 1);
}
template<class SettingsLike>
std::string controller_mode_override(const SettingsLike& settings, unsigned player) {
    const auto mode = settings.get(controller_mode_key(player), "inherit");
    return mode == "classic" || mode == "modern" ? mode : "inherit";
}
template<class SettingsLike>
bool controller_is_modern(const SettingsLike& settings, unsigned player) {
    auto mode = controller_mode_override(settings, player);
    if (mode == "inherit") mode = settings.get("controller_mode", "classic");
    return mode == "modern";
}
}  // namespace dreamcomp
