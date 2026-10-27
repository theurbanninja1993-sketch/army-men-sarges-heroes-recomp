#pragma once
#include <cstdint>
namespace dream { class Memory; class System; }
void army_set_aspect(dream::System& sys, float aspect);
void army_camera_aspect(dream::Memory& memory, std::uint32_t camera);
