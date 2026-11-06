// The same translated guest units are shared with the enhanced experimental target.
// Stock bring-up deliberately observes the hook without changing guest state.
#include "dream/runtime/sh4/abi.h"
namespace dream::gen {
bool dream_hook_entry_pal_boot_language(dream::sh4::Ctx&, dream::Memory&) { return false; }
bool dream_hook_entry_logo_status(dream::sh4::Ctx&, dream::Memory&) { return false; }
bool dream_hook_entry_camera_update(dream::sh4::Ctx&, dream::Memory&) { return false; }
bool dream_hook_entry_projection_submit(dream::sh4::Ctx&, dream::Memory&) { return false; }
void dream_hook_exit_projection_submit(dream::sh4::Ctx&, dream::Memory&) {}
}
