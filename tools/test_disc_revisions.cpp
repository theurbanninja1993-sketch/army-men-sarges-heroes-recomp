// Tests use synthetic SH-4 code. Optional disc arguments remain local, never fixtures in Git.
#include "dream/translator/config/game_config.h"
#include "dream/translator/emit.h"
#include "launcher_model.h"
#include "dream/runtime/host_ext.h"
#include "dream/runtime/system.h"
#include "dream/runtime/sh4/abi.h"
#include <cstring>
namespace dream::gen {
bool dream_hook_entry_snow_intro_audio(dream::sh4::Ctx&, dream::Memory&);
bool dream_hook_entry_snow_intro_play(dream::sh4::Ctx&, dream::Memory&);
}
#include <iostream>
#include <stdexcept>

using namespace dream::translator;
using namespace dreamcomp::frontend;
static const std::string cdi_sha = "51384b12a9de6d9a44caecaadafd29dca86807cf";
static const std::string usa_sha = "5f49f40a0a30fe240d8868025201326b266e369b";
static void require(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}
static std::string config(const std::string& list, const std::string& literals = "[0x0C01000C]") {
    return "[game]\nid=\"test\"\ntitle=\"Synthetic\"\n[disc]\nsha1_1st_read=\"" + cdi_sha +
        "\"\ncompatible_sha1_1st_read=" + list +
        "\n[binary]\npath=\"unused\"\nload_address=0x8C010000\nlink_address=0x0C010000\n"
        "entry=0x8C010000\nruntime_literals=" + literals + "\n";
}
int main(int argc, char** argv) try {
    GameConfig cfg; std::string err;
    require(parse_game_config(config("[\"" + usa_sha + "\"]"), ".", cfg, err), err);
    require(cfg.accepts_boot_sha1(cdi_sha) && cfg.accepts_boot_sha1(usa_sha), "accept both verified hashes");
    require(!cfg.accepts_boot_sha1(std::string(40, '0')), "reject unknown hash");
    require(cfg.runtime_literals == std::vector<std::uint32_t>{0x0c01000c}, "parse runtime literals");
    for (const auto& bad : {"\"not-an-array\"", "[\"short\"]", "[7]", "[\"ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ\"]"}) {
        GameConfig reject;
        require(!parse_game_config(config(bad), ".", reject, err), "reject malformed compatible SHA-1 list");
    }
    for (const auto& bad : {"[0x0C010001]", "[-1]", "\"not-an-array\""}) {
        GameConfig reject;
        require(!parse_game_config(config("[]", bad), ".", reject, err), "reject malformed runtime literals");
    }
    Image image;
    image.base = 0x0c010000;
    // Two PC-relative loads, rts/nop, then independent 32-bit pool values.
    const std::uint16_t words[] = {0xd002, 0xd103, 0x000b, 0x0009, 0x0009, 0x0009,
                                  150, 0, 1234, 0};
    for (auto w : words) { image.bytes.push_back(w & 255); image.bytes.push_back(w >> 8); }
    FunctionSpec f; f.entry = image.base; f.end = image.base + 8;
    EmitOptions options; options.fp_entry_clones = false;
    const auto folded = emit_unit(image, {f}, options).source;
    require(folded.find("c.r[0] = 0x00000096u") != std::string::npos, "baseline folds first pool");
    options.runtime_literals = {image.base + 12};
    const auto dynamic = emit_unit(image, {f}, options).source;
    require(dynamic.find("c.r[0] = m.read32(0x0c01000cu)") != std::string::npos, "explicit pool read at runtime");
    require(dynamic.find("c.r[1] = 0x000004d2u") != std::string::npos, "unrelated pool still folds");
    image.bytes[12] = 45150 & 255; image.bytes[13] = 45150 >> 8;
    require(emit_unit(image, {f}, options).source == dynamic, "one translation handles different pool values");
    options.runtime_literals.clear();
    require(emit_unit(image, {f}, options).source != folded, "regression detects stale baked-in value");
    // A secondary executable can reuse a function prefix and change its pointer pool.
    // --no-fold must not silently bake that pool back into a direct JSR call.
    Image calls; calls.base = 0x0c010000;
    const std::uint16_t call_words[] = {0xd302, 0x430b, 0x0009, 0x000b,
                                       0x0009, 0x0009, 0x0010, 0x0c01,
                                       0x000b, 0x0009};
    for (auto w : call_words) { calls.bytes.push_back(w & 255); calls.bytes.push_back(w >> 8); }
    FunctionSpec caller; caller.entry = calls.base; caller.end = calls.base + 10;
    FunctionSpec callee; callee.entry = calls.base + 16; callee.end = calls.base + 20;
    EmitOptions nofold; nofold.fold_literals = false;
    const auto indirect = emit_unit(calls, {caller, callee}, nofold);
    require(indirect.direct_calls == 0 &&
            indirect.source.find("call_indirect(c, m, target)") != std::string::npos,
            "no-fold JSR observes the live target register");
    Image long_code; long_code.base = 0x0c010000;
    for (unsigned i = 0; i < 40; ++i) {
        long_code.bytes.push_back(9); long_code.bytes.push_back(0);
    }
    long_code.bytes[76] = 0x0b;
    FunctionSpec long_fn; long_fn.entry = long_code.base; long_fn.end = long_code.base + 80;
    EmitOptions overlay; overlay.overlay = true;
    const auto prefix = emit_unit(long_code, {long_fn}, overlay).source;
    long_code.bytes[70] = 8;
    const auto changed_prefix = emit_unit(long_code, {long_fn}, overlay).source;
    require(changed_prefix.substr(changed_prefix.find("const std::uint16_t kSig_")) ==
            prefix.substr(prefix.find("const std::uint16_t kSig_")),
            "short overlay signatures cannot distinguish a change beyond 64 bytes");
    overlay.overlay_full_signature = true;
    const auto full = emit_unit(long_code, {long_fn}, overlay).source;
    long_code.bytes[70] = 9;
    const auto other_full = emit_unit(long_code, {long_fn}, overlay).source;
    require(full.substr(full.find("const std::uint16_t kSig_")) !=
            other_full.substr(other_full.find("const std::uint16_t kSig_")),
            "whole-function signatures distinguish overlapping program bodies");
    // A loop split at a discovered entry must not grow the host stack each iteration.
    Image split; split.base = 0x0c010000;
    const std::uint16_t split_words[] = {0xe064, 0x70ff, 0x2008, 0x8bfc, 0x000b, 0x0009};
    for (auto w : split_words) { split.bytes.push_back(w & 255); split.bytes.push_back(w >> 8); }
    FunctionSpec first; first.entry = split.base; first.end = split.base + 4;
    FunctionSpec second; second.entry = split.base + 4; second.end = split.base + 12;
    const auto split_source = emit_unit(split, {first, second}, overlay).source;
    require(split_source.find("tail_transfer(c, m, 0x0c010004u)") != std::string::npos &&
            split_source.find("tail_transfer(c, m, 0x0c010002u)") != std::string::npos,
            "split overlay loops unwind on both fallthrough and conditional tail edges");
    Image tail; tail.base = 0x0c010000;
    const std::uint16_t tail_words[] = {0xd001, 0x402b, 0x0009, 0x0009,
                                      0x0010, 0x0c01, 0x0009, 0x0009, 0x000b, 0x0009};
    for (auto w : tail_words) { tail.bytes.push_back(w & 255); tail.bytes.push_back(w >> 8); }
    FunctionSpec hooked; hooked.entry = tail.base; hooked.end = tail.base + 6;
    hooked.hook = "synthetic_tail"; hooked.hook_entry = false; hooked.hook_exit = true;
    const auto tail_source = emit_unit(tail, {hooked}, overlay).source;
    require(tail_source.find("call_indirect(c, m, target); return;") != std::string::npos &&
            tail_source.find("tail_transfer(c, m, target)") == std::string::npos &&
            tail_source.find("dream_hook_exit_synthetic_tail(c, m)") != std::string::npos,
            "hooked tail calls finish before running their exit cleanup");
    std::cout << "PASS: dynamic JSR targets and full executable-overlay signatures\n";
    // A synthetic cinematic cue: retail keeps its native stream/cue; CDI retains the enhancement.
    dream::System sys;
    auto& m = sys.memory;
    const dream::sh4::FunctionEntry native_entry{0x8c004000,
        +[](dream::sh4::Ctx& c, dream::Memory&) { c.r[0] = 0x12345678; }};
    dream::sh4::register_functions(&native_entry, 1);
    sys.ctx.r[0] = 0;
    dream::sh4::resume_at(sys.ctx, m, native_entry.address);
    require(sys.ctx.r[0] == 0x12345678, "tail resume dispatches native entries without a guest extent");
    // Live signatures must notice changes beyond a short prefix, including through RAM aliases.
    std::vector<std::uint16_t> signature(512, 0x0009);
    constexpr std::uint32_t signed_address = 0x0c320000;
    for (std::size_t i = 0; i < signature.size(); ++i)
        m.write16(signed_address + static_cast<std::uint32_t>(2 * i), signature[i]);
    const dream::sh4::FunctionEntry signed_entry{signed_address,
        native_entry.fn, signature.data(), static_cast<std::uint32_t>(signature.size()),
        signed_address + static_cast<std::uint32_t>(2 * signature.size())};
    dream::sh4::register_functions(&signed_entry, 1);
    for (bool fast : {false, true}) {
        m.refresh_fast_path();
        if (!fast) { m.fast_ram = nullptr; m.fast_stores = false; }
        require(dream::sh4::find_function(0x8c320000, &m) == native_entry.fn,
                "full signature matches through RAM alias");
        m.write16(signed_address + 900, 0xffff);
        require(dream::sh4::find_function(0x8c320000, &m) == nullptr,
                "changed late signature word invalidates dispatch immediately");
        m.write16(signed_address + 900, 0x0009);
        require(dream::sh4::find_function(signed_address, &m) == native_entry.fn,
                "restored body dispatches without stale signature cache");
    }
    m.refresh_fast_path();
    const dream::sh4::FunctionEntry tail_entries[] = {
        {0x0c330000, +[](dream::sh4::Ctx& c, dream::Memory& memory) {
            if (++c.r[0] < 100000)
                dream::sh4::tail_transfer(c, memory, 0x0c330000);
        }},
        {0x0c330100, +[](dream::sh4::Ctx& c, dream::Memory& memory) {
            dream::sh4::call_indirect(c, memory, 0x0c330200);
            c.r[1] = 0xbad;
        }},
        {0x0c330200, +[](dream::sh4::Ctx& c, dream::Memory& memory) {
            dream::sh4::tail_transfer(c, memory, 0x0c330000);
        }},
        {0x0c330300, +[](dream::sh4::Ctx& c, dream::Memory& memory) {
            dream::sh4::run_guest(c, memory, 0x0c330000);
            c.r[2] = 42;
        }}
    };
    dream::sh4::register_functions(tail_entries, 4);
    sys.ctx.pr = 0x0cffffff;
    sys.ctx.r[0] = 0; sys.ctx.r[1] = 0;
    dream::sh4::run_guest(sys.ctx, m, 0x0c330100);
    require(sys.ctx.r[0] == 100000 && sys.ctx.r[1] == 0,
            "nested tail unwinds caller, then repeated tails use bounded runner dispatch");
    sys.ctx.r[0] = 0; sys.ctx.r[2] = 0;
    dream::sh4::run_guest(sys.ctx, m, 0x0c330300);
    require(sys.ctx.r[0] == 100000 && sys.ctx.r[2] == 42,
            "nested runner restores outer runner after deferred tail transfers");
    std::cout << "PASS: live RAM signatures, bounded tail loops and nested runner unwinding\n";
    dream::host::Extension* soundtrack = nullptr;
    for (auto* e : dream::host::extensions())
        if (std::strcmp(e->name(), "Army Men Snow soundtrack selection") == 0) soundtrack = e;
    require(soundtrack != nullptr, "soundtrack extension registered");
    const std::uint32_t header = 0x8c300000, cue = header + 0x100;
    for (bool retail : {false, true}) {
        m.write32(0x8c135eb8, retail ? 45166 : 11866);
        soundtrack->on_start(sys);
        m.write32(0x8c18ed20, 8); m.write32(0x8c17e338, header);
        m.write32(header, 0); m.write32(header + 4, 10533);
        m.write32(header + 32, 1); m.write32(header + 36, cue);
        m.write32(cue, 0); m.write32(cue + 4, 4000); m.write32(cue + 8, 5);
        m.write16(cue + 12, 23); m.write16(cue + 14, 100); m.write32(cue + 16, 1);
        dream::gen::dream_hook_entry_snow_intro_audio(sys.ctx, m);
        require(m.read32(cue + 4) == (retail ? 4000u : 6000u) &&
                m.read16(cue + 12) == (retail ? 23 : 33), "native versus replacement Snow cue");
        m.write16(cue + 12, 33); m.write32(cue + 16, 0);
        m.write32(0x8c17e3c0, 2000); m.write32(0x8c17e4e8, 7);
        sys.ctx.pr = 0x0c055c4c; sys.ctx.r[5] = 33; sys.ctx.r[6] = 0;
        dream::gen::dream_hook_entry_snow_intro_play(sys.ctx, m);
        require(sys.ctx.r[6] == (retail ? 0u : 85u), "retail soundtrack keeps native SFX mute");
        soundtrack->on_stop(sys, "test");
    }
    std::cout << "PASS: native retail Snow soundtrack and CDI enhancement remain separate\n";
    std::cout << "PASS: revision allowlist, strict parsing, selective runtime literal reads\n";
    if (argc >= 3) {
        const auto cdi = verify_disc(argv[1], cdi_sha, {usa_sha});
        const auto usa = verify_disc(argv[2], cdi_sha, {usa_sha});
        require(cdi.status == DiscStatus::Verified && cdi.found_sha1 == cdi_sha, "launcher accepts CDI");
        require(usa.status == DiscStatus::Verified && usa.found_sha1 == usa_sha, "launcher accepts USA retail");
        require(verify_disc(argv[2], cdi_sha).status == DiscStatus::WrongRevision, "retail needs explicit allowlist");
        require(verify_disc(argv[1], std::string(40, '0')).status == DiscStatus::WrongRevision, "unknown remains rejected");
        if (argc == 4) {
            require(verify_disc(argv[3], cdi_sha, {usa_sha}).status == DiscStatus::WrongRevision,
                    "NTSC runtime allowlist must still reject PAL");
            require(verify_disc(argv[3], cdi_sha,
                    {usa_sha, "7e1288a58f26eedf76cc7bdbaeb53a72a9332fbc"}).status == DiscStatus::Verified,
                    "combined launcher can accept the separately translated PAL loader");
        }
        std::cout << "PASS: actual CDI/USA launcher verification and negative cases\n";
    }
    return 0;
} catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
