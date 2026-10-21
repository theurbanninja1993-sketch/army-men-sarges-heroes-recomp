// Army Men: Sarge's Heroes: what this port tells dreamcomp. Game facts with sources: docs/GAME-INTERNALS.md.
#include "dreamcomp/hook.h"
#include "dreamcomp/port.h"

namespace {

const dreamcomp::PortInfo kInfo = [] {
    dreamcomp::PortInfo p;
    p.id = "armymensh";
    p.title = "Army Men: Sarge's Heroes";
    return p;
}();
dreamcomp::RegisterPort g_register(kInfo);

}  // namespace
