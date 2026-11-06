// Read-only diagnostics shared by the isolated PAL targets; no gameplay patches.
#include "dream/runtime/host_ext.h"
#include "dream/runtime/system.h"
#include <cstdlib>

namespace {
struct PalDiagnostics final : dream::host::Extension {
    bool trace = false;
    const char* name() const override { return "PAL bring-up diagnostics"; }
    void on_start(dream::System&) override { trace = std::getenv("ARMY_PAL_TRACE") != nullptr; }
    void on_vblank(dream::System& sys) override {
        const auto frame = sys.spg.frames();
        if (trace && frame % 600 == 0)
            std::fprintf(stderr, "PAL: frame %llu pc %08x entry target %08x\n",
                         static_cast<unsigned long long>(frame), sys.ctx.pc,
                         sys.memory.read32(0x8c01002c));
    }
} diagnostics;
dream::host::Register registration(diagnostics);
}
