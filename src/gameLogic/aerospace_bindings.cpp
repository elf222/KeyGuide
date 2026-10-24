// gameLogic/aerospace_bindings.cpp
#include "gameLogic/aerospace_bindings.hpp"
#include <array>
#include <deque>
#include <string>
#include <utility>
#include <vector>

namespace
{
    struct BindingStore
    {
        // Labels need stable storage for the string_views; deque never moves its elements.
        std::deque<std::string> labels;
        std::vector<AeroSpaceBinding> bindings;
        bool defaults = true;

        void add(Platform::KeyId key, std::uint8_t modifiers, std::string short_hint,
                 std::string full_hint, bool service, bool enters_service, bool returns_to_main)
        {
            using K = Platform::KeyId;
            char32_t mnemonic = 0;
            const bool letter = key >= K::A && key <= K::Z;
            if (letter && short_hint.find(static_cast<char>(key) + 32) != std::string::npos)
            {
                mnemonic = static_cast<char32_t>(key) + 32;
            }
            else if ((letter || (key >= K::Zero && key <= K::Nine)) &&
                     short_hint.find(static_cast<char>(key)) != std::string::npos)
            {
                mnemonic = static_cast<char32_t>(key);
            }
            const std::string_view short_view = labels.emplace_back(std::move(short_hint));
            const std::string_view full_view = labels.emplace_back(std::move(full_hint));
            bindings.push_back({key, modifiers, service, short_view, full_view, mnemonic,
                                enters_service, returns_to_main});
        }
    };

    BindingStore DefaultBindings()
    {
        using K = Platform::KeyId;
        using namespace Platform;
        BindingStore store;
        const auto main = [&](K key, std::uint8_t modifiers, std::string short_hint,
                              std::string full_hint)
        {
            store.add(key, modifiers, std::move(short_hint), std::move(full_hint), false, false,
                      false);
        };
        const auto service = [&](K key, std::uint8_t modifiers, std::string short_hint,
                                 std::string full_hint)
        {
            store.add(key, modifiers, std::move(short_hint), std::move(full_hint), true, false,
                      true);
        };
        main(K::Slash, OPTION, "tiles", "tiles layout; toggle horizontal / vertical");
        main(K::Comma, OPTION, "accordion", "accordion layout; toggle horizontal / vertical");
        const std::array<std::pair<K, std::string_view>, 4> directions{
            {{K::H, "left"}, {K::J, "down"}, {K::K, "up"}, {K::L, "right"}}};
        for (const auto& [key, direction] : directions)
        {
            const auto name = std::string(direction);
            main(key, OPTION, name, "focus the window " + name);
            main(key, OPTION | SHIFT, "move", "move the window " + name);
            service(key, OPTION | SHIFT, "join",
                    "join with the window " + name + "; back to main mode");
        }
        main(K::Minus, OPTION, "shrink", "resize smart -50");
        main(K::Equal, OPTION, "grow", "resize smart +50");
        for (const char workspace : std::string_view("123456789ABCDEFGIMNOPQRSTUVWXYZ"))
        {
            const auto key = static_cast<K>(workspace);
            const auto name = std::string(1, workspace);
            main(key, OPTION, "ws " + name, "switch to workspace " + name);
            main(key, OPTION | SHIFT, "send " + name, "move the window to workspace " + name);
        }
        main(K::Tab, OPTION, "back", "previous workspace (back and forth)");
        main(K::Tab, OPTION | SHIFT, "monitor", "move the workspace to the next monitor");
        store.add(K::Semicolon, OPTION | SHIFT, "service",
                  "service mode: Esc reload, r reset, f float, Backspace close others", false,
                  true, false);
        service(K::Escape, 0, "reload", "reload config; back to main mode");
        service(K::R, 0, "reset", "reset layout (flatten workspace tree); back to main mode");
        service(K::F, 0, "float", "toggle floating / tiling; back to main mode");
        service(K::Backspace, 0, "close others",
                "close all windows but current; back to main mode");
        return store;
    }

    BindingStore& Active()
    {
        static BindingStore store = DefaultBindings();
        return store;
    }
} // namespace

std::span<const AeroSpaceBinding> AeroSpaceBindings()
{
    return Active().bindings;
}

void SetAeroSpaceBindings(std::span<const AeroSpaceConfigBinding> bindings)
{
    BindingStore store;
    store.defaults = false;
    for (const auto& binding : bindings)
    {
        store.add(binding.key, binding.modifiers, binding.shortHint, binding.fullHint,
                  binding.service, binding.entersService, binding.returnsToMain);
    }
    Active() = std::move(store);
}

void RestoreDefaultAeroSpaceBindings()
{
    Active() = DefaultBindings();
}

bool AeroSpaceUsesDefaults()
{
    return Active().defaults;
}

bool AeroSpaceServiceAfter(bool service, Platform::KeyId key, std::uint8_t modifiers)
{
    for (const auto& binding : AeroSpaceBindings())
    {
        if (binding.service == service && binding.key == key && binding.modifiers == modifiers)
        {
            return service ? !binding.returnsToMain : binding.entersService;
        }
    }
    return service;
}
