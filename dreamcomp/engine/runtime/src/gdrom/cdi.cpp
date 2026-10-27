// DiscJuggler v2/v3/v3.5: sector payloads followed by a little-endian session directory.
#include "dream/runtime/gdrom/disc.h"

#include <array>
#include <fstream>
#include <limits>

namespace dream::gdrom {
namespace {
class CdiDisc final : public Disc {
    struct Payload {
        std::uint64_t offset;
        std::uint32_t stride, user_offset;
    };
    std::ifstream file_;
    std::vector<Payload> payloads_;

public:
    bool open(const std::filesystem::path& path, std::string& error) {
        path_ = path.string();
        file_.open(path, std::ios::binary);
        auto fail = [&](const char* reason) {
            error = std::string("CDI: ") + reason;
            return false;
        };
        if (!file_) return fail("cannot open image");
        file_.seekg(0, std::ios::end);
        const auto end = file_.tellg();
        if (end < 8) return fail("truncated footer");
        const auto size = static_cast<std::uint64_t>(end);
        file_.seekg(end - std::streamoff(8));
        auto integer = [&](unsigned bytes) {
            std::uint32_t value = 0;
            for (unsigned i = 0; i < bytes; ++i) {
                char byte = 0;
                file_.get(byte);
                value |= std::uint32_t(static_cast<unsigned char>(byte)) << (8 * i);
            }
            return value;
        };
        const auto version = integer(4), directory = integer(4);
        constexpr std::uint32_t v2 = 0x80000004, v3 = 0x80000005, v35 = 0x80000006;
        if (version != v2 && version != v3 && version != v35)
            return fail("unsupported DiscJuggler version");
        if (!directory || directory > size - 8) return fail("invalid directory offset");
        const std::uint64_t header = version == v35 ? size - directory : directory;
        if (header >= size - 8) return fail("invalid directory offset");
        file_.seekg(static_cast<std::streamoff>(header));
        // All directory operations stay before the footer, including optional metadata blocks.
        auto skip = [&](std::uint32_t n) { file_.ignore(n); };
        auto valid = [&]() {
            const auto pos = file_.tellg();
            return file_ && pos >= 0 && static_cast<std::uint64_t>(pos) <= size - 8;
        };
        const auto sessions = integer(2);
        if (!sessions || sessions > 99) return fail("invalid session count");
        std::uint64_t payload = 0;
        for (unsigned session = 0; session < sessions; ++session) {
            const auto count = integer(2);
            if (count > 99 - tracks_.size()) return fail("invalid track count");
            for (unsigned i = 0; i < count; ++i) {
                if (integer(4)) skip(8);
                constexpr std::array<unsigned char, 10> marker{0, 0, 1, 0, 0, 0, 255, 255, 255, 255};
                for (unsigned copy = 0; copy < 2; ++copy) {
                    std::array<unsigned char, 10> found{};
                    file_.read(reinterpret_cast<char*>(found.data()), found.size());
                    if (!file_ || found != marker) return fail("invalid track marker");
                }
                skip(4);
                const auto filename_size = integer(1);
                skip(filename_size + 19);
                if (integer(4) == 0x80000000) skip(8);
                skip(2);
                const auto pregap = integer(4), length = integer(4);
                skip(6);
                const auto mode = integer(4);
                skip(12);
                const auto start = integer(4), total = integer(4);
                skip(16);
                const auto format = integer(4);
                std::uint32_t stride = 0;
                switch (format) {
                    case 0: stride = 2048; break;
                    case 1: stride = 2336; break;
                    case 2: stride = 2352; break;
                    case 4: stride = 2448; break;
                    default: return fail("unsupported sector size");
                }
                skip(29);
                if (version != v2) {
                    skip(5);
                    if (integer(4) == 0xffffffff) skip(78);
                }
                if (!valid()) return fail("truncated track directory");
                const std::uint64_t fad = std::uint64_t(start) + pregap;
                const auto bytes = std::uint64_t(total) * stride;
                if (mode > 2 || (!mode && stride < 2352) ||
                    (stride == 2336 && mode != 2))
                    return fail("unsupported track mode");
                if (!length || std::uint64_t(pregap) + length > total ||
                    fad < kFadOffset || fad - kFadOffset + length >
                        std::numeric_limits<std::uint32_t>::max() ||
                    payload > header || bytes > header - payload)
                    return fail("invalid or truncated track payload");
                Track t;
                t.number = static_cast<unsigned>(tracks_.size() + 1);
                t.lba = static_cast<std::uint32_t>(fad - kFadOffset);
                t.sectors = length;
                t.data = mode != 0;
                // Strip Mode 2 headers and subchannel bytes here. The runtime sees ordinary
                // 2048-byte data sectors or 2352-byte audio, as it does for a converted GDI.
                t.sector_size = t.data ? 2048 : 2352;
                const std::uint32_t user_offset = !t.data || stride == 2048 ? 0 :
                    stride == 2336 ? 8 : mode == 2 ? 24 : 16;
                if (!tracks_.empty() && t.lba < tracks_.back().end_lba())
                    return fail("overlapping tracks");
                tracks_.push_back(t);
                payloads_.push_back({payload + std::uint64_t(pregap) * stride, stride, user_offset});
                payload += bytes;
            }
            skip(version == v2 ? 12 : 13);
            if (!valid()) return fail("truncated session directory");
        }
        if (tracks_.empty()) return fail("image contains no tracks");
        finish_tracks();
        return true;
    }

    bool read_raw(std::uint32_t lba, std::uint8_t* out) override {
        const auto* track = track_for(lba);
        if (!track) return false;
        const auto& p = payloads_[track->number - 1];
        const auto offset = p.offset + std::uint64_t(lba - track->lba) * p.stride + p.user_offset;
        file_.clear();
        file_.seekg(static_cast<std::streamoff>(offset));
        return bool(file_.read(reinterpret_cast<char*>(out), track->sector_size));
    }
};
}  // namespace

std::unique_ptr<Disc> open_cdi(const std::filesystem::path& path, std::string& error) {
    auto disc = std::make_unique<CdiDisc>();
    return disc->open(path, error) ? std::unique_ptr<Disc>(std::move(disc)) : nullptr;
}
}  // namespace dream::gdrom
