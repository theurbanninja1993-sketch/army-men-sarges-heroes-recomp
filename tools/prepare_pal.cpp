// Local developer extraction for the experimental PAL build. Never ship these outputs.
#include "dream/runtime/gdrom/disc.h"
#include "dream/runtime/sha1.h"
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) try {
    if (argc != 3) {
        std::cerr << "usage: armymensh_pal_extract DISC_IMAGE OUTPUT_DIRECTORY\n";
        return 2;
    }
    struct Input {
        const char* path;
        const char* sha;
        const char* output;
        std::vector<std::uint8_t> bytes;
        std::size_t offset = 0;
    };
    std::array<Input, 3> files{{
        {"1ST_READ.BIN", "7e1288a58f26eedf76cc7bdbaeb53a72a9332fbc", "1ST_READ.BIN", {}},
        {"KLINK/KLINKENGLISH60HZ.BIN", "7e81e3336ab4d5dd6c7094f00e388ef4d9d12192",
         "KLINKENGLISH60HZ.BIN", {}},
        // The verified transfer container has an 8192-byte header. Its 244-byte code
        // payload exactly matches RAM at 0x8CFD2000 after the original loader runs.
        {"GINXFER.BIN", "08e56771a4b0ff8dd6cf2896902e94fed7402fd0",
         "GINXFER.CODE.BIN", {}, 0x2000}
    }};
    std::string error;
    auto disc = dream::gdrom::open_disc(argv[1], error);
    if (!disc) throw std::runtime_error(error);
    // Validate the complete set before creating the output directory or writing a file.
    for (auto& file : files) {
        if (!dream::gdrom::read_root_file(*disc, file.path, file.bytes))
            throw std::runtime_error(std::string("missing PAL file: ") + file.path);
        const auto sha = dream::sha1_hex(file.bytes.data(), file.bytes.size());
        if (sha != file.sha)
            throw std::runtime_error(std::string(file.path) + " has unsupported SHA-1 " + sha);
        if (file.offset >= file.bytes.size()) throw std::runtime_error("invalid payload offset");
    }
    const std::filesystem::path out = argv[2];
    std::filesystem::create_directories(out);
    for (const auto& file : files) {
        const auto path = out / file.output;
        std::ofstream stream(path, std::ios::binary);
        stream.write(reinterpret_cast<const char*>(file.bytes.data() + file.offset),
                     static_cast<std::streamsize>(file.bytes.size() - file.offset));
        if (!stream) throw std::runtime_error("cannot write " + path.string());
        std::cout << file.output << ": " << file.bytes.size() - file.offset << " bytes, SHA-1 "
                  << dream::sha1_hex(file.bytes.data() + file.offset, file.bytes.size() - file.offset) << '\n';
    }
    return 0;
} catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
}
