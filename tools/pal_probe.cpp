// The generic boot harness requires a registered entry even with --interpret.
// No PAL instructions or USA hooks are linked into this diagnostic executable.
#include "dream/runtime/sh4/abi.h"
#include <cstdio>
#include <cstdlib>

namespace {
void interpreter_required(dream::sh4::Ctx&, dream::Memory&) {
    std::fputs("PAL probe requires --interpret; no native PAL translation is linked.\n", stderr);
    std::exit(2);
}
const dream::sh4::FunctionEntry entry{0x0c010000, interpreter_required};
struct Register {
    Register() { dream::sh4::register_functions(&entry, 1); }
} registration;
}
