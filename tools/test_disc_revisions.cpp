// Tests use synthetic SH-4 code. Optional disc arguments remain local, never fixtures in Git.
#include "dream/translator/config/game_config.h"
#include "dream/translator/emit.h"
#include "launcher_model.h"
#include "dream/runtime/host_ext.h"
#include "dream/runtime/system.h"
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
    // A synthetic cinematic cue: retail keeps its native stream/cue; CDI retains the enhancement.
    dream::System sys;
    auto& m = sys.memory;
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
        if (argc == 4)
            require(verify_disc(argv[3], cdi_sha, {usa_sha}).status == DiscStatus::WrongRevision,
                    "PAL remains rejected until a separate translation is implemented");
        std::cout << "PASS: actual CDI/USA launcher verification and negative cases\n";
    }
    return 0;
} catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
