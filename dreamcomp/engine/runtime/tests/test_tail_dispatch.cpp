#include <doctest.h>
#include <cstdlib>
#include <exception>
#include "dream/runtime/sh4/abi.h"

namespace {
using namespace dream;
using namespace dream::sh4;
constexpr std::uint32_t caller_pc = 0x0c700000, return_pc = caller_pc + 4;
constexpr std::uint32_t chain_pc = 0x0c700100, escape_pc = 0x0c700200;
constexpr std::uint32_t inside_pc = 0x0c700304;
unsigned unwinds = 0;
struct WatchUnwind {
    int initial = std::uncaught_exceptions();
    ~WatchUnwind() { if (std::uncaught_exceptions() > initial) ++unwinds; }
};
void caller_resume(Ctx& c, Memory& m, std::uint32_t pc) {
    WatchUnwind watch;
    if (pc != return_pc) {
        c.pr = return_pc;
        call_indirect(c, m, c.r[1]);
    }
    ++c.r[2];
    c.pr = 0;
}
void caller(Ctx& c, Memory& m) { caller_resume(c, m, 0); }
void chain(Ctx& c, Memory& m) {
    if (--c.r[0]) tail_transfer(c, m, chain_pc);
    else if (c.r[3]) tail_transfer(c, m, inside_pc);
}
void escape(Ctx& c, Memory& m) { nonlocal_return(c, m, 0); }
void interior(Ctx&, Memory&) { FAIL("interior tail must use the resume entry"); }
void interior_resume(Ctx& c, Memory& m, std::uint32_t pc) {
    CHECK(pc == inside_pc);
    ++c.r[4];
    if (c.r[3] == 2) { c.pr = 0; return; }
    tail_transfer(c, m, c.pr);
}
void setup() {
    static bool registered = false;
    if (registered) return;
    registered = true;
    const FunctionEntry table[] = {
        {caller_pc, caller, nullptr, 0, caller_pc + 8, caller_resume},
        {chain_pc, chain}, {escape_pc, escape},
        {inside_pc - 4, interior, nullptr, 0, inside_pc + 4, interior_resume}};
    register_functions(table, 4);
}
}

TEST_CASE("nested overlay tail chain returns to its caller without growing the stack") {
    setup();
    dream::BareMemory m;
    dream::sh4::Ctx c{};
    c.pr = 0; c.r[0] = 100000; c.r[1] = chain_pc;
    unwinds = 0;
    dream::sh4::run_guest(c, m, caller_pc);
    CHECK(c.r[0] == 0);
    CHECK(c.r[2] == 1);
    const char* flag = std::getenv("DREAM_LOCAL_TAILS");
    CHECK(unwinds == (flag && *flag == '1' ? 0u : 1u));
}

TEST_CASE("true nonlocal return still abandons nested callers and restores tail runners") {
    setup();
    dream::BareMemory m;
    dream::sh4::Ctx c{};
    c.pr = 0; c.r[1] = escape_pc;
    unwinds = 0;
    dream::sh4::run_guest(c, m, caller_pc);
    CHECK(c.r[2] == 0);
    CHECK(unwinds == 1);
    // A second entry checks the runner/depth restored during the exception unwind.
    c.pr = 0; c.r[0] = 100000; c.r[1] = chain_pc;
    dream::sh4::run_guest(c, m, caller_pc);
    CHECK(c.r[0] == 0);
    CHECK(c.r[2] == 1);
}

TEST_CASE("nested tails resume an interior block then tail back to the guest caller") {
    setup();
    dream::BareMemory m;
    dream::sh4::Ctx c{};
    c.pr = 0; c.r[0] = 2; c.r[1] = chain_pc; c.r[3] = 1;
    dream::sh4::run_guest(c, m, caller_pc);
    CHECK(c.r[4] == 1);
    CHECK(c.r[2] == 1);
}

TEST_CASE("resumed tail with a changed return PC abandons its original call") {
    setup();
    dream::BareMemory m;
    dream::sh4::Ctx c{};
    c.pr = 0; c.r[0] = 1; c.r[1] = chain_pc; c.r[3] = 2;
    dream::sh4::run_guest(c, m, caller_pc);
    CHECK(c.r[4] == 1);
    CHECK(c.r[2] == 0);
}

TEST_CASE("delivered interrupt calls keep the original unwind behavior") {
    struct InterruptHooks : dream::sh4::Hooks {
        void on_poll(dream::sh4::Ctx&, dream::Memory&) override {}
        void on_trapa(dream::sh4::Ctx&, dream::Memory&, std::uint32_t) override {}
        void on_rte(dream::sh4::Ctx&, dream::Memory&) override {}
        void on_untranslated(dream::sh4::Ctx&, dream::Memory&, std::uint32_t) override {
            FAIL("unexpected untranslated target");
        }
        bool in_exception_frame() const noexcept override { return true; }
    } irq;
    setup();
    dream::sh4::set_hooks(&irq);
    struct RestoreHooks { ~RestoreHooks() { dream::sh4::set_hooks(nullptr); } } restore;
    dream::BareMemory m;
    dream::sh4::Ctx c{};
    c.pr = 0; c.r[0] = 100; c.r[1] = chain_pc;
    unwinds = 0;
    dream::sh4::run_guest(c, m, caller_pc);
    CHECK(c.r[0] == 0);
    CHECK(c.r[2] == 1);
    CHECK(unwinds == 1);
}
