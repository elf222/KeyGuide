// platform/input_event.hpp
#pragma once
#include <cstddef>
#include <cstdint>
#include "platform/blender_observation.hpp"

namespace Platform
{
    enum class AppProfile
    {
        Zed,
        Neovim,
        Blender
    };
    enum class KeyId : std::uint16_t
    {
        Unknown = 0,
        Space = 32,
        Apostrophe = 39,
        Comma = 44,
        Minus = 45,
        Period = 46,
        Slash = 47,
        Zero = 48,
        One,
        Two,
        Three,
        Four,
        Five,
        Six,
        Seven,
        Eight,
        Nine,
        Semicolon = 59,
        Equal = 61,
        A = 65,
        B,
        C,
        D,
        E,
        F,
        G,
        H,
        I,
        J,
        K,
        L,
        M,
        N,
        O,
        P,
        Q,
        R,
        S,
        T,
        U,
        V,
        W,
        X,
        Y,
        Z,
        LeftBracket = 91,
        Backslash,
        RightBracket,
        Grave = 96,
        Escape = 256,
        Enter,
        Tab,
        Backspace,
        Delete,
        Right,
        Left,
        Down,
        Up,
        PageUp,
        PageDown,
        Home,
        End,
        Shift,
        Control,
        Option,
        Command,
        RightShift,
        RightControl,
        RightOption,
        RightCommand,
        CapsLock,
        F1,
        F2,
        F3,
        F4,
        F5,
        F6,
        F7,
        F8,
        F9,
        F10,
        F11,
        F12,
        NumpadZero,
        NumpadOne,
        NumpadTwo,
        NumpadThree,
        NumpadFour,
        NumpadFive,
        NumpadSix,
        NumpadSeven,
        NumpadEight,
        NumpadNine,
        NumpadPeriod,
        NumpadDivide,
        NumpadMultiply,
        NumpadMinus,
        NumpadPlus,
        NumpadEnter,
        NumpadEqual,
        Count
    };
    enum class SpecialKey : std::uint8_t
    {
        None,
        Escape,
        Enter,
        Tab,
        Backspace,
        Delete,
        Right,
        Left,
        Down,
        Up,
        PageUp,
        PageDown,
        Home,
        End
    };

    struct KeyValue
    {
        char32_t character = 0;
        SpecialKey special = SpecialKey::None;
        constexpr bool operator==(const KeyValue&) const = default;
    };

    inline constexpr std::uint8_t SHIFT = 1, CONTROL = 2, OPTION = 4, COMMAND = 8;
    inline constexpr std::size_t KEY_COUNT = static_cast<std::size_t>(KeyId::Count);

    constexpr KeyId FunctionKeyForNumber(KeyId key)
    {
        if (key >= KeyId::One && key <= KeyId::Nine)
        {
            return static_cast<KeyId>(static_cast<int>(KeyId::F1) + static_cast<int>(key) -
                                      static_cast<int>(KeyId::One));
        }
        switch (key)
        {
            case KeyId::Zero:
                return KeyId::F10;
            case KeyId::Minus:
                return KeyId::F11;
            case KeyId::Equal:
                return KeyId::F12;
            default:
                return KeyId::Unknown;
        }
    }

    constexpr std::uint8_t ModifierBit(KeyId key)
    {
        switch (key)
        {
            case KeyId::Shift:
            case KeyId::RightShift:
                return SHIFT;
            case KeyId::Control:
            case KeyId::RightControl:
                return CONTROL;
            case KeyId::Option:
            case KeyId::RightOption:
                return OPTION;
            case KeyId::Command:
            case KeyId::RightCommand:
                return COMMAND;
            default:
                return 0;
        }
    }
    enum class MouseButton : std::uint8_t
    {
        Unknown,
        Left,
        Right,
        Middle,
        Other
    };
    enum class EventType : std::uint8_t
    {
        KeyDown,
        KeyUp,
        Modifiers,
        MouseAction,
        Unsupported,
        NormalBaseline,
        InsertBaseline,
        VisualBaseline,
        ReplaceBaseline,
        ObjectBaseline,
        MeshEditBaseline,
        CurveEditBaseline,
        NodeEditorBaseline
    };

    struct InputEvent
    {
        EventType type = EventType::KeyDown;
        KeyId key = KeyId::Unknown;
        KeyValue value{};
        std::uint8_t modifiers = 0;
        bool repeat = false;
        bool capsLock = false;
        bool functionHeld = false;
        double timestamp = 0;
        std::uint64_t sequence = 0;
        std::uint64_t foregroundGeneration = 0;
        std::uint64_t lossGeneration = 0;
        const char* baselineReason = nullptr;
        MouseButton mouseButton = MouseButton::Unknown;
        bool mousePressed = true;
    };

    struct InputBatchInfo
    {
        std::size_t count = 0;
        std::uint64_t foregroundGeneration = 0;
        std::uint64_t lossGeneration = 0;
        bool foreground = false;
        bool available = false;
        bool preview = false;
        bool paused = false;
        bool quit = false;
        bool reduceMotion = false;
        const char* description = "Preview";
        AppProfile appProfile = AppProfile::Zed;
        bool macCommandAliases = false;
        BlenderObservation blenderObservation{};
    };

    struct PointerPosition
    {
        float x = 0, y = 0;
        bool inside = false;
    };
} // namespace Platform
