#pragma once
#include <cstdint>

// Addresses belong to executable profiles, not disc container formats.
namespace army::layout {
inline constexpr std::uint32_t player_camera_stride = 0x310;
#ifdef ARMY_PAL_EN60
inline constexpr std::uint32_t logo_status_caller = 0x0c073724;
inline constexpr std::uint32_t movie_state = 0x8c0f0ab0;
inline constexpr std::uint32_t midway_filename = 0x8c0f0af0;
inline constexpr std::uint32_t saffire_filename = 0x8c0f0aff;
inline constexpr std::uint32_t newly_pressed_buttons = 0x8c1fe738;
inline constexpr std::uint32_t mission_phase = 0x8c1a6174;
inline constexpr std::uint32_t menu_inhibit = 0x8c1a5bc0;
inline constexpr std::uint32_t follow_camera = 0x0c05be4c;
inline constexpr std::uint32_t aim_camera = 0x0c05c290;
inline constexpr std::uint32_t projection_caller = 0x0c05b6f2;
inline constexpr std::uint32_t frontend_screen = 0x8c0ce8d0;
inline constexpr std::uint32_t frontend_submenu = 0x8c18f850;
inline constexpr std::uint32_t save_dialog = 0x8c1a4968;
inline constexpr std::uint32_t extra_save_prompt = 2; // PAL no-VM dialogue has its buttons lower down
inline constexpr std::uint32_t menu_help = 0x8c18f270;
inline constexpr std::uint32_t active_player = 0x8c18d780;
inline constexpr std::uint32_t panel_table = 0x8c0d3dc0;
inline constexpr std::uint32_t load_disabled = 0x8c18f4a8;
inline constexpr std::uint32_t main_row = 0x8c18f260;
inline constexpr std::uint32_t options_row = 0x8c18f268;
inline constexpr std::uint32_t password_buffer = 0x8c18f7c8;
inline constexpr std::uint32_t password_length = 0x8c18f7e0;
inline constexpr std::uint32_t password_row = 0x8c18f7e8;
inline constexpr std::uint32_t password_column = 0x8c18f7f0;
inline constexpr std::uint32_t password_result = 0x8c18f7d8;
inline constexpr std::uint32_t empty_message = 0x8c0d0e21;
#else
inline constexpr std::uint32_t logo_status_caller = 0x0c073074;
inline constexpr std::uint32_t movie_state = 0x8c0f01e0;
inline constexpr std::uint32_t midway_filename = 0x8c0f0220;
inline constexpr std::uint32_t saffire_filename = 0x8c0f022f;
inline constexpr std::uint32_t newly_pressed_buttons = 0x8c1fdbf8;
inline constexpr std::uint32_t mission_phase = 0x8c18ed1c;
inline constexpr std::uint32_t menu_inhibit = 0x8c18e770;
inline constexpr std::uint32_t follow_camera = 0x0c057964;
inline constexpr std::uint32_t aim_camera = 0x0c057da8;
inline constexpr std::uint32_t projection_caller = 0x0c05720a;
inline constexpr std::uint32_t frontend_screen = 0x8c0eb1d0;
inline constexpr std::uint32_t frontend_submenu = 0x8c1a53c0;
inline constexpr std::uint32_t save_dialog = 0x8c1a3c28;
inline constexpr std::uint32_t extra_save_prompt = 0;
inline constexpr std::uint32_t menu_help = 0x8c1a4df0;
inline constexpr std::uint32_t active_player = 0x8c18cc70;
inline constexpr std::uint32_t panel_table = 0x8c0cfda0;
inline constexpr std::uint32_t load_disabled = 0x8c1a5018;
inline constexpr std::uint32_t main_row = 0x8c1a4de0;
inline constexpr std::uint32_t options_row = 0x8c1a4de8;
inline constexpr std::uint32_t password_buffer = 0x8c1a5338;
inline constexpr std::uint32_t password_length = 0x8c1a5350;
inline constexpr std::uint32_t password_row = 0x8c1a5358;
inline constexpr std::uint32_t password_column = 0x8c1a5360;
inline constexpr std::uint32_t password_result = 0x8c1a5348;
inline constexpr std::uint32_t empty_message = 0x8c0ed6f3;
#endif
}
