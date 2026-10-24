// gameLogic/shortcut_bindings.cpp
#include "gameLogic/shortcut_bindings.hpp"
#include "gameLogic/aerospace_bindings.hpp"
#include "gameLogic/blender_bindings.hpp"
#include "gameLogic/vim_bindings.hpp"

namespace
{
    // Hint ids are laid out as Vim, then Blender, then AeroSpace bindings.
    std::size_t AeroSpaceOffset()
    {
        return VimBindings().size() + BlenderBindings().size();
    }

    bool PhysicalMatch(Platform::KeyId binding_key, Platform::KeyId key,
                       const std::array<Platform::KeyValue, 2>& values)
    {
        const bool escape_alias =
            key == Platform::KeyId::CapsLock && values[0].special == Platform::SpecialKey::Escape;
        return binding_key == key || (escape_alias && binding_key == Platform::KeyId::Escape);
    }
} // namespace

std::size_t ShortcutCount()
{
    return AeroSpaceOffset() + AeroSpaceBindings().size();
}

ShortcutHint GetShortcutHint(HintId id)
{
    if (id >= ShortcutCount())
    {
        return {};
    }
    const auto vim = VimBindings();
    if (id < vim.size())
    {
        const auto& binding = vim[id];
        return {binding.shortHint, binding.fullHint,  binding.chord,
                binding.mnemonic,  binding.modifiers, binding.scope == BindingScope::Ex};
    }
    if (id >= AeroSpaceOffset())
    {
        const auto& binding = AeroSpaceBindings()[id - AeroSpaceOffset()];
        return {binding.shortHint,
                binding.fullHint,
                {},
                binding.mnemonic,
                binding.modifiers,
                false,
                true};
    }
    const auto& binding = BlenderBindings()[id - vim.size()];
    return {
        binding.shortHint, binding.fullHint, {}, binding.mnemonic, binding.modifiers, false, true};
}

bool ShortcutMatchesKey(HintId id, Platform::KeyId key,
                        const std::array<Platform::KeyValue, 2>& values, std::uint8_t modifiers)
{
    if (id >= ShortcutCount())
    {
        return false;
    }
    const auto vim = VimBindings();
    if (id < vim.size())
    {
        const auto& binding = vim[id];
        if (binding.modifiers != modifiers)
        {
            return false;
        }
        const auto value = values[(modifiers & Platform::SHIFT) ? 1 : 0];
        return (binding.value == value &&
                (value.character || value.special != Platform::SpecialKey::None)) ||
               (binding.scope == BindingScope::Argument && value.character >= 32) ||
               (binding.scope == BindingScope::LiteralArgument &&
                (value.character || value.special != Platform::SpecialKey::None));
    }
    if (id >= AeroSpaceOffset())
    {
        const auto& binding = AeroSpaceBindings()[id - AeroSpaceOffset()];
        return PhysicalMatch(binding.key, key, values) && binding.modifiers == modifiers;
    }
    const auto& binding = BlenderBindings()[id - vim.size()];
    return PhysicalMatch(binding.key, key, values) &&
           (binding.anyModifiers || binding.modifiers == modifiers);
}

bool ShortcutAvailable(HintId id, const TrackedState& state)
{
    if (id >= ShortcutCount())
    {
        return false;
    }
    const auto vim = VimBindings();
    if (id < vim.size())
    {
        return state.appProfile != Platform::AppProfile::Blender &&
               BindingAvailable(vim[id], state.vim);
    }
    if (id >= AeroSpaceOffset())
    {
        return false;
    }
    return state.appProfile == Platform::AppProfile::Blender &&
           BlenderBindingAvailable(BlenderBindings()[id - vim.size()], state.blender);
}

bool AeroSpaceShortcutAvailable(HintId id, bool service)
{
    return id >= AeroSpaceOffset() && id < ShortcutCount() &&
           AeroSpaceBindings()[id - AeroSpaceOffset()].service == service;
}
