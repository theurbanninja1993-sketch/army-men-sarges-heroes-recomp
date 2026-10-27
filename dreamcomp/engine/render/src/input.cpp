#include "dream/render/input.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace dream::render {
namespace {

struct Entry {
    PadControl c;
    const char* key;    // in the file
    const char* label;  // in the UI
};

// The file keys are a promise: change one and everybody's binding for it silently reverts.
constexpr Entry kEntries[] = {
    {PadControl::A, "a", "A"},
    {PadControl::B, "b", "B"},
    {PadControl::X, "x", "X"},
    {PadControl::Y, "y", "Y"},
    {PadControl::Start, "start", "START"},
    {PadControl::Up, "dpad_up", "D-PAD UP"},
    {PadControl::Down, "dpad_down", "D-PAD DOWN"},
    {PadControl::Left, "dpad_left", "D-PAD LEFT"},
    {PadControl::Right, "dpad_right", "D-PAD RIGHT"},
    {PadControl::LeftTrigger, "ltrigger", "L TRIGGER"},
    {PadControl::RightTrigger, "rtrigger", "R TRIGGER"},
    {PadControl::StickUp, "stick_up", "STICK UP"},
    {PadControl::StickDown, "stick_down", "STICK DOWN"},
    {PadControl::StickLeft, "stick_left", "STICK LEFT"},
    {PadControl::StickRight, "stick_right", "STICK RIGHT"},
};
static_assert(sizeof kEntries / sizeof kEntries[0] == kPadControlCount,
              "every PadControl needs a file key and a label");

std::string trim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) s.remove_prefix(1);
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) s.remove_suffix(1);
    return std::string(s);
}

std::string upper(std::string s) {
    for (char& ch : s) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    return s;
}

}  // namespace

const char* pad_control_label(PadControl c) {
    return kEntries[static_cast<unsigned>(c)].label;
}

const char* pad_control_key(PadControl c) {
    return kEntries[static_cast<unsigned>(c)].key;
}

bool pad_control_from_key(std::string_view key, PadControl& out) {
    for (const Entry& e : kEntries) {
        if (key == e.key) {
            out = e.c;
            return true;
        }
    }
    return false;
}

std::string Binding::text() const {
    switch (source) {
        case BindSource::Key:
            return "key:" + code;
        case BindSource::Button:
            return "button:" + code;
        case BindSource::Axis:
            return "axis:" + code + (sign < 0 ? "-" : "+");
        case BindSource::Mouse:
            return "mouse:" + code;
        case BindSource::None:
            break;
    }
    return {};
}

bool Binding::parse(std::string_view text, Binding& out) {
    const auto colon = text.find(':');
    if (colon == std::string_view::npos)
        return false;
    const std::string_view kind = text.substr(0, colon);
    std::string code(text.substr(colon + 1));
    if (code.empty())
        return false;
    out = Binding{};
    if (kind == "key") {
        out.source = BindSource::Key;
    } else if (kind == "button") {
        out.source = BindSource::Button;
    } else if (kind == "mouse") {
        out.source = BindSource::Mouse;
    } else if (kind == "axis") {
        out.source = BindSource::Axis;
        // The sign is the last character, and it is required: an axis without a half is ambiguous
        // and guessing "+" would bind the wrong direction half the time.
        if (code.back() == '-') {
            out.sign = -1;
        } else if (code.back() == '+') {
            out.sign = 1;
        } else {
            return false;
        }
        code.pop_back();
        if (code.empty())
            return false;
    } else {
        return false;
    }
    out.code = code;
    return true;
}

std::string Binding::label() const {
    if (!bound())
        return "--";
    if (source == BindSource::Axis)
        return upper(code) + (sign < 0 ? " -" : " +");
    return upper(code);
}

Bindings Bindings::defaults() {
    Bindings k;
    auto key = [](const char* name) { return Binding{BindSource::Key, name, 1}; };
    // The keyboard table the launcher has always had, so a first run with no file behaves exactly
    // as before this existed.
    auto mouse = [](const char* name) { return Binding{BindSource::Mouse, name, 1}; };
    // The keyboard-and-mouse layout for a PC player (dreamcomp), mapped onto the Dreamcast pad as
    // the original manual does: stick to move, X to shoot, A to act (jump), B to change weapon, Y
    // to change stance, L trigger to aim in first person, R trigger to strafe, Start to pause.
    // Arrow keys stay on the d-pad and the stick (second slot) for menus and anyone who prefers them.
    k.keyboard[PadControl::A] = key("Space");
    k.keyboard[PadControl::B] = key("E");
    k.keyboard[PadControl::X] = mouse("left");
    k.keyboard[PadControl::Y] = key("C");
    // Enter, not Escape: Escape opens the settings panel (dreamcomp's core extension).
    k.keyboard[PadControl::Start] = key("Return");
    k.keyboard[PadControl::Up] = key("Up");
    k.keyboard[PadControl::Down] = key("Down");
    k.keyboard[PadControl::Left] = key("Left");
    k.keyboard[PadControl::Right] = key("Right");
    k.keyboard[PadControl::LeftTrigger] = mouse("right");
    k.keyboard[PadControl::RightTrigger] = key("Q");
    // The stick is WASD, with the arrow keys as its second slot.
    k.keyboard[PadControl::StickUp] = key("W");
    k.keyboard[PadControl::StickDown] = key("S");
    k.keyboard[PadControl::StickLeft] = key("A");
    k.keyboard[PadControl::StickRight] = key("D");
    k.keyboard.slot(PadControl::StickUp, 1) = key("Up");
    k.keyboard.slot(PadControl::StickDown, 1) = key("Down");
    k.keyboard.slot(PadControl::StickLeft, 1) = key("Left");
    k.keyboard.slot(PadControl::StickRight, 1) = key("Right");

    auto btn = [](const char* name) { return Binding{BindSource::Button, name, 1}; };
    auto axis = [](const char* name, int sign) { return Binding{BindSource::Axis, name, sign}; };
    // SDL's own gamepad names, so any pad it recognises works on first plug-in with no UI visit.
    // The Dreamcast's A and B sit where a modern pad's A and B do; X and Y likewise.
    k.gamepad[PadControl::A] = btn("a");
    k.gamepad[PadControl::B] = btn("b");
    k.gamepad[PadControl::X] = btn("x");
    k.gamepad[PadControl::Y] = btn("y");
    k.gamepad[PadControl::Start] = btn("start");
    k.gamepad[PadControl::Up] = btn("dpup");
    k.gamepad[PadControl::Down] = btn("dpdown");
    k.gamepad[PadControl::Left] = btn("dpleft");
    k.gamepad[PadControl::Right] = btn("dpright");
    k.gamepad[PadControl::LeftTrigger] = axis("lefttrigger", 1);
    k.gamepad[PadControl::RightTrigger] = axis("righttrigger", 1);
    k.gamepad[PadControl::StickUp] = axis("lefty", -1);
    k.gamepad[PadControl::StickDown] = axis("lefty", 1);
    k.gamepad[PadControl::StickLeft] = axis("leftx", -1);
    k.gamepad[PadControl::StickRight] = axis("leftx", 1);
    return k;
}

std::string Bindings::to_text() const {
    std::ostringstream o;
    o << "# dream-recomp input bindings. Rewritten whenever they change in the UI.\n"
      << "# Physical inputs are SDL's own names, so this file survives an SDL upgrade.\n"
      << "version = 1\n"
      << "deadzone = " << deadzone_percent << "\n"
      << "trigger_ramp = " << (trigger_ramp ? 1 : 0) << "\n";
    auto section = [&o](const char* name, const DeviceBindings& d) {
        o << "\n[" << name << "]\n";
        for (unsigned i = 0; i < kPadControlCount; ++i) {
            const auto c = static_cast<PadControl>(i);
            o << pad_control_key(c) << " = " << d[c].text() << "\n";
        }
        // The second slots (dreamcomp), only where bound: "a.2 = key:Space".
        for (unsigned i = 0; i < kPadControlCount; ++i) {
            const auto c = static_cast<PadControl>(i);
            if (d.slot(c, 1).bound())
                o << pad_control_key(c) << ".2 = " << d.slot(c, 1).text() << "\n";
        }
    };
    section("keyboard", keyboard);
    section("gamepad", gamepad);
    return o.str();
}

Bindings Bindings::from_text(std::string_view text, std::string* warnings) {
    // Start from the defaults rather than from nothing: a file that sets half the controls leaves
    // the other half working, and a file that is complete nonsense leaves the game playable. A
    // binding UI whose config file can brick the controls is worse than no binding UI.
    Bindings out = Bindings::defaults();
    auto warn = [warnings](const std::string& s) {
        if (warnings)
            *warnings += s + "\n";
    };
    DeviceBindings* current = nullptr;
    std::string line;
    std::istringstream in{std::string(text)};
    unsigned lineno = 0;
    while (std::getline(in, line)) {
        ++lineno;
        const std::string s = trim(line);
        if (s.empty() || s[0] == '#')
            continue;
        if (s.front() == '[' && s.back() == ']') {
            const std::string name = trim(s.substr(1, s.size() - 2));
            if (name == "keyboard")
                current = &out.keyboard;
            else if (name == "gamepad")
                current = &out.gamepad;
            else {
                current = nullptr;
                warn("line " + std::to_string(lineno) + ": unknown section [" + name + "]");
            }
            continue;
        }
        const auto eq = s.find('=');
        if (eq == std::string::npos) {
            warn("line " + std::to_string(lineno) + ": not a setting");
            continue;
        }
        const std::string key = trim(s.substr(0, eq));
        const std::string value = trim(s.substr(eq + 1));
        if (!current) {
            if (key == "deadzone") {
                // Clamped rather than rejected: a nonsensical number should not cost the whole
                // file, and a 100% dead zone would mean a stick that never moves.
                try {
                    out.deadzone_percent = std::clamp(std::stoi(value), 0, 90);
                } catch (...) {
                    warn("line " + std::to_string(lineno) + ": deadzone is not a number: " + value);
                }
            } else if (key == "trigger_ramp") {
                out.trigger_ramp = value != "0";
            } else if (key != "version") {
                warn("line " + std::to_string(lineno) + ": unknown setting " + key);
            }
            continue;
        }
        PadControl c{};
        unsigned slot = 0;
        std::string name = key;
        if (name.size() > 2 && name.compare(name.size() - 2, 2, ".2") == 0) {
            name.resize(name.size() - 2);
            slot = 1;
        }
        if (!pad_control_from_key(name, c)) {
            warn("line " + std::to_string(lineno) + ": unknown control " + key);
            continue;
        }
        if (value.empty()) {
            current->slot(c, slot) = Binding{};  // deliberately unbound
            continue;
        }
        Binding b;
        if (!Binding::parse(value, b)) {
            warn("line " + std::to_string(lineno) + ": cannot read binding " + value);
            continue;
        }
        current->slot(c, slot) = b;
    }
    return out;
}

}  // namespace dream::render
