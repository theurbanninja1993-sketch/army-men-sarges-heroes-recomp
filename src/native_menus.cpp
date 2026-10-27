// Native menus keep their artwork, selection state and password validator.
#include "dream/runtime/host_ext.h"
#include "dream/runtime/system.h"
#include "dream/render/input.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace {
using dream::render::PadControl;
constexpr char alphabet[] = "BCDFGHJKLMNPQRSTVWXZ";
constexpr std::uint32_t buffer = 0x8c1a5338u, length = 0x8c1a5350u;
constexpr std::uint32_t key_row = 0x8c1a5358u, key_column = 0x8c1a5360u;
enum class Kind { None, Main, Options, Password, Panel, Title, SavePrompt };
struct Context {
    Kind kind = Kind::None;
    std::uint32_t panel = 0;
    bool help = false;
    bool operator==(const Context&) const = default;
};
enum class Op { Move, Click, Back, WheelUp, WheelDown, Text, Erase, Clear, Enter, Up, Down, Left, Right };
struct Action { Op op; float x = -1, y = -1; std::string text; Context context; };
struct Scheduled { std::uint64_t frame; Action action; bool sdl = false; };
bool ram(std::uint32_t p, unsigned size) {
    return (p & 0xff000000u) == 0x8c000000u && (p & 0xffffffu) <= 0x1000000u - size;
}
Context context(dream::Memory& m) {
    const auto phase = m.read32(0x8c18ed1cu);
    // Initial title/frontend uses 0; returning menus use 7. The screen index
    // remains stale in gameplay, so it cannot identify an active menu alone.
    if ((phase == 0 || phase == 7) && m.read32(0x8c0eb1d0u) == 7 && m.read32(0x8c1a3c28u) == 5)
        return {Kind::SavePrompt, 5, false};
    if ((phase == 0 || phase == 7) && m.read32(0x8c0eb1d0u) == 4) return {Kind::Title, 0, false};
    if ((phase == 0 || phase == 7) && m.read32(0x8c0eb1d0u) == 5) {
        const auto sub = m.read32(0x8c1a53c0u);
        if (sub <= 2) return {sub == 0 ? Kind::Main : sub == 1 ? Kind::Options : Kind::Password,
                              0, m.read8(0x8c1a4df0u) != 0};
    }
    if (phase == 3) {
        const auto player = m.read32(0x8c18cc70u);
        if (player > 0 && player <= 4) {
            const auto p = m.read32(0x8c0cfda0u + 4 * player);
            if (ram(p, 28) && m.read32(p) && m.read32(p + 16) > 0 &&
                m.read32(p + 16) <= 32 && ram(p, 28 + 24 * m.read32(p + 16)))
                return {Kind::Panel, p, false};
        }
    }
    return {};
}
bool host_menu() {
    for (auto* e : dream::host::extensions())
        if (e->overlay_open() || e->guest_paused()) return true;
    return false;
}

class NativeMenus final : public dream::host::Extension {
    Context current_;
    std::mutex mutex_;
    std::deque<Action> actions_;
    std::deque<PadControl> buttons_;
    PadControl pulse_ = PadControl::A;
    unsigned pulse_ticks_ = 0, release_ticks_ = 0;
    std::vector<Scheduled> script_;
    std::size_t script_at_ = 0;
    bool trace_ = false, cancel_input_ = false;

    void button(PadControl b) { if (buttons_.size() < 64) buttons_.push_back(b); }
    void erase(dream::Memory& m, bool all) {
        auto n = std::min(m.read32(length), 12u);
        n = all ? 0 : n ? n - 1 : 0;
        m.write32(length, n); m.write8(buffer + n, 0);
    }
    bool point(dream::Memory& m, float x, float y) {
        if (current_.help || !std::isfinite(x) || !std::isfinite(y)) return false;
        if (current_.kind == Kind::SavePrompt)
            return y >= 88 && y <= 106 && ((x >= 40 && x <= 102) || (x >= 113 && x <= 280));
        if (current_.kind == Kind::Title) return x >= 0 && x <= 320 && y >= 0 && y <= 240;
        if (current_.kind == Kind::Main || current_.kind == Kind::Options) {
            const int row = int(std::floor((y - 48) / 21));
            const int count = current_.kind == Kind::Main ? 6 : 5;
            if (x < 148 || x > 284 || row < 0 || row >= count || y > 64.f + static_cast<float>(row) * 21.f)
                return false;
            if (current_.kind == Kind::Main && row == 5 && m.read8(0x8c1a5018u)) return false;
            m.write32(current_.kind == Kind::Main ? 0x8c1a4de0u : 0x8c1a4de8u, row);
            return true;
        }
        if (current_.kind == Kind::Password) {
            if (x >= 151.5f && x < 276.5f && y >= 56 && y < 90) {
                m.write32(key_row, y < 73.5f ? 0 : 1);
                m.write32(key_column, unsigned((x - 151.5f) / 12.5f));
                return true;
            }
            if (y >= 92 && y <= 104) {
                if ((x >= 163 && x <= 204) || (x >= 229 && x <= 273)) {
                    m.write32(key_row, 2); m.write32(key_column, x < 210 ? 0 : 1);
                    return true;
                }
            }
        }
        if (current_.kind == Kind::Panel) {
            const auto p = current_.panel;
            const auto count = m.read32(p + 16);
            for (unsigned i = 0; i < count; ++i) {
                const auto entry = p + 28 + 24 * i;
                const auto xy = m.read32(entry + 8);
                const auto px = static_cast<std::int16_t>(xy & 0xffff);
                const auto py = static_cast<std::int16_t>(xy >> 16);
                const float left = px >= 140 ? px - 80.f : px - 6.f;
                if (x >= left && x <= 285 && y >= py - 3 && y <= py + 16) {
                    m.write32(p + 8, i); return true;
                }
            }
        }
        return false;
    }
    void apply(dream::Memory& m, const Action& a) {
        if (!(a.context == current_) || current_.kind == Kind::None) return;
        if (a.op == Op::Back) { button(current_.help ? PadControl::RightTrigger : PadControl::B); return; }
        if (current_.help) { if (a.op == Op::Click) button(PadControl::RightTrigger); return; }
        if (a.op == Op::Move) { point(m, a.x, a.y); return; }
        if (a.op == Op::Click) {
            if (!point(m, a.x, a.y)) return;
            if (current_.kind == Kind::SavePrompt) button(a.x < 110 ? PadControl::A : PadControl::B);
            else if (current_.kind == Kind::Title) button(PadControl::Start);
            else if (current_.kind == Kind::Options) button(a.x < 220 ? PadControl::Left : PadControl::Right);
            else if (current_.kind == Kind::Main && m.read32(0x8c1a4de0u) == 0) button(PadControl::Right);
            else if (current_.kind == Kind::Panel) {
                const auto i = m.read32(current_.panel + 8);
                const auto value = m.read32(current_.panel + 28 + 24 * i + 12);
                button(value ? (a.x < 210 ? PadControl::Left : PadControl::Right) : PadControl::A);
            } else button(PadControl::A);
        } else if (a.op == Op::WheelUp || a.op == Op::WheelDown) {
            point(m, a.x, a.y);
            button(a.op == Op::WheelUp ? PadControl::Right : PadControl::Left);
        } else if (current_.kind == Kind::Password) {
            if (a.op == Op::Text) {
                auto n = std::min(m.read32(length), 12u);
                for (unsigned char ch : a.text) {
                    if (ch >= 'a' && ch <= 'z') ch -= 'a' - 'A';
                    if (n < 12 && std::string_view(alphabet).find(char(ch)) != std::string_view::npos)
                        m.write8(buffer + n++, ch);
                }
                m.write8(buffer + n, 0); m.write32(length, n);
                m.write32(0x8c1a5348u, 0x8c0ed6f3u); // clear the previous result message
            } else if (a.op == Op::Erase || a.op == Op::Clear) erase(m, a.op == Op::Clear);
            else if (a.op == Op::Enter) {
                m.write32(key_row, 2); m.write32(key_column, 1); button(PadControl::A);
            } else if (a.op >= Op::Up && a.op <= Op::Right) {
                button(a.op == Op::Up ? PadControl::Up : a.op == Op::Down ? PadControl::Down :
                       a.op == Op::Left ? PadControl::Left : PadControl::Right);
            }
        }
    }
    void enqueue(Action a) {
        std::lock_guard lock(mutex_);
        if (current_.kind == Kind::None || actions_.size() >= 256) return;
        if (!(a.context == current_)) return;
        if (a.op == Op::Move && !actions_.empty() && actions_.back().op == Op::Move)
            actions_.back() = std::move(a);
        else actions_.push_back(std::move(a));
    }
public:
    const char* name() const override { return "Army Men native menu input"; }
    bool wants_overlay() override { return true; } // event access; no host overlay or guest pause
    bool on_event(const void* event) override {
        const auto& e = *static_cast<const SDL_Event*>(event);
        if (host_menu()) return false;
        Context c;
        { std::lock_guard lock(mutex_); c = current_; }
        if (c.kind == Kind::None) return false;
        if (e.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
            std::lock_guard lock(mutex_); actions_.clear(); cancel_input_ = true; return false;
        }
        if (e.type == SDL_EVENT_MOUSE_MOTION || e.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
            e.type == SDL_EVENT_MOUSE_WHEEL) {
            const float wx = e.type == SDL_EVENT_MOUSE_MOTION ? e.motion.x :
                             e.type == SDL_EVENT_MOUSE_WHEEL ? e.wheel.mouse_x : e.button.x;
            const float wy = e.type == SDL_EVENT_MOUSE_MOTION ? e.motion.y :
                             e.type == SDL_EVENT_MOUSE_WHEEL ? e.wheel.mouse_y : e.button.y;
            auto& hc = dream::host::host_controls();
            float x = 0, y = 0;
            if (!hc.normalize_pointer || !hc.normalize_pointer(wx, wy, x, y)) return false;
            const float aspect = hc.window_aspect ? hc.window_aspect() : 4.f / 3.f;
            // The native menu art/HUD uses a centred 4:3 viewport, also in Expanded.
            if (aspect > 4.f/3.f) x = (x - .5f) * aspect / (4.f/3.f) + .5f;
            else if (aspect > 0) y = (y - .5f) * (4.f/3.f) / aspect + .5f;
            Op op = Op::Move;
            if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                if (e.button.button == SDL_BUTTON_RIGHT) op = Op::Back;
                else if (e.button.button == SDL_BUTTON_LEFT) op = Op::Click;
                else return false;
            } else if (e.type == SDL_EVENT_MOUSE_WHEEL) {
                if (e.wheel.y == 0) return true;
                const float dy = e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -e.wheel.y : e.wheel.y;
                op = dy > 0 ? Op::WheelUp : Op::WheelDown;
            }
            enqueue({op, x * 320, y * 240, {}, c}); return true;
        }
        if (e.type == SDL_EVENT_KEY_DOWN && c.kind == Kind::Password) {
            const auto key = e.key.key;
            if (e.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_ALT | SDL_KMOD_GUI)) return false;
            if ((key >= 'a' && key <= 'z') || (key >= 'A' && key <= 'Z')) { enqueue({Op::Text, -1, -1, std::string(1,char(key)), c}); return true; }
            const Op op = key == SDLK_BACKSPACE ? Op::Erase : key == SDLK_DELETE ? Op::Clear :
                          key == SDLK_RETURN || key == SDLK_KP_ENTER ? Op::Enter :
                          key == SDLK_UP ? Op::Up : key == SDLK_DOWN ? Op::Down :
                          key == SDLK_LEFT ? Op::Left : key == SDLK_RIGHT ? Op::Right : Op::Move;
            if (op != Op::Move) { enqueue({op, -1, -1, {}, c}); return true; }
        }
        return false;
    }
    bool map_keyboard(const dream::render::KeyboardSample& physical,
                      dream::render::GamepadSample& logical) override {
        if (current_.kind == Kind::None) return false;
        logical = {};
        if (current_.kind != Kind::Password) logical.controls = physical.controls;
        // Native menus consume brief button pulses, not a held mouse button. Hold
        // for two polls to cover 30 Hz menu logic, then release before another click.
        if (!pulse_ticks_ && !release_ticks_ && !buttons_.empty()) {
            pulse_ = buttons_.front(); buttons_.pop_front(); pulse_ticks_ = 2;
        }
        if (pulse_ticks_) {
            logical.controls[unsigned(pulse_)] = 1;
            if (!--pulse_ticks_) release_ticks_ = 2;
        } else if (release_ticks_) --release_ticks_;
        return true;
    }
    void on_start(dream::System&) override {
        current_ = {}; actions_.clear(); buttons_.clear(); script_.clear();
        pulse_ticks_ = release_ticks_ = 0; script_at_ = 0; cancel_input_ = false;
        trace_ = std::getenv("ARMY_TRACE_MENU");
        // Deterministic native-input diagnostic, analogous to ARMY_TEST_MOUSE.
        if (const char* path = std::getenv("ARMY_MENU_SCRIPT")) {
            std::ifstream file(path); std::string line;
            while (std::getline(file, line)) {
                std::istringstream in(line); Scheduled s{}; std::string op;
                if (!(in >> s.frame >> op)) continue;
                if (op.starts_with("sdl_")) { s.sdl = true; op.erase(0, 4); }
                s.action.op = op == "click" ? Op::Click : op == "move" ? Op::Move :
                              op == "text" ? Op::Text : op == "enter" ? Op::Enter :
                              op == "erase" ? Op::Erase : op == "clear" ? Op::Clear :
                              op == "back" ? Op::Back : Op::WheelUp;
                if (op == "click" || op == "move") in >> s.action.x >> s.action.y;
                if (op == "text") in >> s.action.text;
                script_.push_back(std::move(s));
            }
        }
    }
    void on_vblank(dream::System& sys) override {
        const auto next = context(sys.memory);
        std::deque<Action> actions;
        {
            std::lock_guard lock(mutex_);
            if (trace_ && !(current_ == next))
                std::printf("Army menu context frame %llu phase %u screen %u sub %u kind %u -> %u pointer %d\n",
                    static_cast<unsigned long long>(sys.spg.frames()),sys.memory.read32(0x8c18ed1cu),
                    sys.memory.read32(0x8c0eb1d0u),sys.memory.read32(0x8c1a53c0u),
                    unsigned(current_.kind),unsigned(next.kind),bool(dream::host::host_controls().set_pointer_input));
            if (!(current_ == next) || host_menu() || cancel_input_) {
                actions_.clear(); buttons_.clear(); pulse_ticks_ = release_ticks_ = 0;
            }
            cancel_input_ = false; current_ = next; actions.swap(actions_);
        }
        auto& hc = dream::host::host_controls();
        if (hc.set_pointer_input) hc.set_pointer_input(next.kind != Kind::None);
        if (host_menu()) return;
        const auto frame = sys.spg.frames();
        while (script_at_ < script_.size() && script_[script_at_].frame <= frame) {
            const auto& step = script_[script_at_++];
            auto a = step.action; a.context = current_;
            if (!step.sdl) { actions.push_back(std::move(a)); continue; }
            // Exercise the actual SDL queue and overlay event filter in window tests.
            SDL_Event event{};
            if (a.op == Op::Text || a.op == Op::Enter || a.op == Op::Erase) {
                event.type = SDL_EVENT_KEY_DOWN;
                event.key.key = a.op == Op::Enter ? SDLK_RETURN : SDLK_BACKSPACE;
                if (a.op == Op::Text) {
                    for (unsigned char ch : a.text) { event.key.key = ch; SDL_PushEvent(&event); }
                } else SDL_PushEvent(&event);
            } else if (a.op == Op::Click || a.op == Op::Back) {
                float nx = 0, ny = 0;
                if (!hc.normalize_pointer || !hc.normalize_pointer(1, 1, nx, ny) || nx <= 0 || ny <= 0) continue;
                const float aspect = hc.window_aspect ? hc.window_aspect() : 4.f/3.f;
                float x = a.x / 320, y = a.y / 240;
                if (aspect > 4.f/3.f) x = (x-.5f) * (4.f/3.f) / aspect + .5f;
                else if (aspect > 0) y = (y-.5f) * aspect / (4.f/3.f) + .5f;
                event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
                event.button.button = a.op == Op::Back ? SDL_BUTTON_RIGHT : SDL_BUTTON_LEFT;
                event.button.x = x / nx; event.button.y = y / ny;
                SDL_PushEvent(&event);
                event.type = SDL_EVENT_MOUSE_BUTTON_UP; SDL_PushEvent(&event);
            }
        }
        for (const auto& a : actions) apply(sys.memory, a);
        if (trace_ && !actions.empty()) {
            char code[13]{};
            for (unsigned i=0;i<12;++i) code[i]=char(sys.memory.read8(buffer+i));
            std::printf("Army menu frame %llu kind %u panel %08x row %u code [%s] length %u\n",
                static_cast<unsigned long long>(frame), unsigned(next.kind), next.panel,
                sys.memory.read32(0x8c1a4de0u),code,sys.memory.read32(length));
        }
    }
};
NativeMenus menus;
dream::host::Register register_menus(menus);
} // namespace
