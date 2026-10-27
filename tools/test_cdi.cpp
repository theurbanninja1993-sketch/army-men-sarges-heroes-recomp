// Synthetic CDI layout/error regressions; optional real CDI/GDI sector comparison.
#include "dream/runtime/gdrom/disc.h"
#include "dream/runtime/sha1.h"
#include "launcher_model.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace dream::gdrom;
using Bytes = std::vector<std::uint8_t>;
static void require(bool ok, const std::string& message) {
    if (!ok) throw std::runtime_error(message);
}
static void put(Bytes& b, std::uint32_t n, unsigned width = 4) {
    for (unsigned i = 0; i < width; ++i) b.push_back(static_cast<std::uint8_t>(n >> (8 * i)));
}
static Bytes fixture(std::uint32_t version, unsigned mode, unsigned format, bool extras) {
    const unsigned stride = format == 0 ? 2048 : format == 1 ? 2336 : format == 2 ? 2352 : 2448;
    const unsigned offset = !mode || stride == 2048 ? 0 : stride == 2336 ? 8 : mode == 2 ? 24 : 16;
    Bytes b(3 * stride, 0xcc);  // one stored pregap sector, two real sectors
    for (unsigned s = 1; s <= 2; ++s)
        std::memset(b.data() + s * stride + offset, int(s + 40), mode ? 2048 : 2352);
    const auto header = b.size();
    put(b, 1, 2); put(b, 1, 2);  // session and track counts
    put(b, extras ? 1 : 0);
    if (extras) b.resize(b.size() + 8);
    for (int i = 0; i < 2; ++i)
        for (auto v : {0, 0, 1, 0, 0, 0, 255, 255, 255, 255}) b.push_back(v);
    b.resize(b.size() + 4); put(b, 3, 1);
    b.insert(b.end(), {'t', 's', 't'}); b.resize(b.size() + 19);
    put(b, extras ? 0x80000000 : 0);
    if (extras) b.resize(b.size() + 8);
    b.resize(b.size() + 2);
    put(b, 1); put(b, 2); b.resize(b.size() + 6); put(b, mode);
    b.resize(b.size() + 12); put(b, 149); put(b, 3); b.resize(b.size() + 16);
    put(b, format); b.resize(b.size() + 29);
    if (version != 0x80000004) {
        b.resize(b.size() + 5); put(b, extras ? 0xffffffff : 0);
        if (extras) b.resize(b.size() + 78);
    }
    b.resize(b.size() + (version == 0x80000004 ? 12 : 13));
    const auto directory = version == 0x80000006 ? b.size() + 8 - header : header;
    put(b, version); put(b, static_cast<std::uint32_t>(directory));
    return b;
}
static void save(const std::filesystem::path& path, const Bytes& b) {
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(b.data()), b.size());
    require(bool(f), "write fixture");
}
int main(int argc, char** argv) try {
    const auto dir = std::filesystem::temp_directory_path() /
        ("army-cdi-tests-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(dir);
    struct Cleanup { std::filesystem::path dir; ~Cleanup() { std::filesystem::remove_all(dir); } } cleanup{dir};
    const auto path = dir / "synthetic.CDI";
    unsigned layouts = 0;
    for (auto version : {0x80000004u, 0x80000005u, 0x80000006u})
        for (unsigned mode = 0; mode < 3; ++mode)
            for (unsigned format : {0u, 1u, 2u, 4u})
                for (bool extras : {false, true}) {
                    if ((!mode && format < 2) || (format == 1 && mode != 2)) continue;
                    save(path, fixture(version, mode, format, extras));
                    std::string err; auto d = open_disc(path, err);
                    require(bool(d), err);
                    require(d->tracks().size() == 1 && d->tracks()[0].lba == 0 &&
                            d->tracks()[0].sectors == 2, "track layout");
                    std::array<std::uint8_t, 2352> raw{};
                    for (unsigned s : {1u, 0u, 1u}) {  // seek both directions
                        require(d->read_raw(s, raw.data()), "read payload");
                        for (unsigned i = 0; i < d->tracks()[0].sector_size; ++i)
                            require(raw[i] == s + 41, "sector normalization / pregap");
                    }
                    require(!d->read_raw(2, raw.data()), "read beyond track");
                    ++layouts;
                }
    auto b = fixture(0x80000006, 2, 1, true);
    unsigned rejected = 0;
    for (std::size_t n : {std::size_t(0), std::size_t(7), b.size() - 1, b.size() - 9}) {
        save(path, Bytes(b.begin(), b.begin() + n));
        std::string err; require(!open_disc(path, err) && !err.empty(), "reject truncated image");
        ++rejected;
    }
    for (int kind = 0; kind < 4; ++kind) {
        auto bad = b;
        if (kind == 0) bad[bad.size() - 8] = 0;  // version
        if (kind == 1) std::fill(bad.end() - 4, bad.end(), 0);  // directory offset
        if (kind == 2) bad[3 * 2336 + 16] = 17;  // track marker
        if (kind == 3) {  // directory would claim a track larger than the stored payload
            const auto original = bad.size();
            bad.erase(bad.begin(), bad.begin() + 2336);
            require(bad.size() < original, "shorten payload");
        }
        save(path, bad); std::string err;
        require(!open_disc(path, err) && !err.empty(), "reject malformed image");
        ++rejected;
    }
    std::cout << layouts << " synthetic layouts and " << rejected << " malformed images passed\n";
    if (argc == 3) {
        std::string err;
        auto cdi = open_disc(argv[1], err); require(bool(cdi), err);
        auto gdi = open_disc(argv[2], err); require(bool(gdi), err);
        require(cdi->tracks().size() == gdi->tracks().size(), "real track count");
        std::uint64_t sectors = 0;
        for (std::size_t i = 0; i < cdi->tracks().size(); ++i) {
            const auto& a = cdi->tracks()[i]; const auto& b = gdi->tracks()[i];
            require(a.lba == b.lba && a.sectors == b.sectors && a.data == b.data &&
                    a.sector_size == b.sector_size, "real track layout");
            std::array<std::uint8_t, 2352> left{}, right{};
            for (auto lba = a.lba; lba < a.end_lba(); ++lba) {
                require(cdi->read_raw(lba, left.data()) && gdi->read_raw(lba, right.data()), "real sector read");
                require(std::memcmp(left.data(), right.data(), a.sector_size) == 0,
                        "sector mismatch at " + std::to_string(lba));
                ++sectors;
            }
        }
        Bytes boot; require(read_root_file(*cdi, "", boot), "read boot file");
        require(dream::sha1_hex(boot.data(), boot.size()) == "51384b12a9de6d9a44caecaadafd29dca86807cf",
                "boot revision");
        using namespace dreamcomp::frontend;
        const auto check = verify_disc(argv[1], "51384b12a9de6d9a44caecaadafd29dca86807cf");
        require(check.status == DiscStatus::Verified, "launcher verification: " + check.detail);
        require(verify_disc(argv[1], "wrong-revision").status == DiscStatus::WrongRevision,
                "launcher rejects wrong revision");
        std::cout << "Launcher accepts CDI and rejects revision mismatch\n";
        std::cout << sectors << " real sectors identical to GDI; boot revision verified\n";
    }
    return 0;
} catch (const std::exception& e) {
    std::cerr << e.what() << '\n'; return 1;
}
