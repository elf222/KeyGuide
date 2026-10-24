// platform/blender_protocol.hpp
#pragma once
#include <algorithm>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include "platform/blender_observation.hpp"

namespace Platform
{
    inline constexpr unsigned short BLENDER_BRIDGE_PORT = 49538;
    inline constexpr std::size_t BLENDER_PACKET_LIMIT = 768;

    struct BlenderMessage
    {
        bool hello = false;
        std::uint32_t processId = 0;
        std::uint64_t sequence = 0, request = 0;
        std::string token, session;
        BlenderObservation observation{};
    };

    inline bool BlenderIdentifier(std::string_view value, std::size_t limit)
    {
        return !value.empty() && value.size() <= limit &&
               std::all_of(value.begin(), value.end(),
                           [](unsigned char c)
                           {
                               return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                                      (c >= '0' && c <= '9') || c == '_';
                           });
    }

    inline std::optional<BlenderMessage> ParseBlenderMessage(std::string_view packet)
    {
        if (packet.empty() || packet.size() > BLENDER_PACKET_LIMIT ||
            !std::all_of(packet.begin(), packet.end(),
                         [](unsigned char c)
                         {
                             return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                                    (c >= '0' && c <= '9') || c == '_' || c == ' ' || c == '\n' ||
                                    c == '\r' || c == '\t';
                         }))
        {
            return std::nullopt;
        }
        std::istringstream input{std::string(packet)};
        BlenderMessage message;
        std::string kind, extra;
        unsigned int version = 0;
        if (!(input >> kind >> version) || version != 1)
        {
            return std::nullopt;
        }
        if (kind == "KC_HELLO")
        {
            message.hello = true;
            if (!(input >> message.processId >> message.session) || !message.processId ||
                !BlenderIdentifier(message.session, 32) || (input >> extra))
            {
                return std::nullopt;
            }
            return message;
        }
        std::string mode, editor, operation;
        unsigned int selection = 0, modal = 0, locked = 0;
        if (kind != "KC_STATE" ||
            !(input >> message.token >> message.processId >> message.session >> message.sequence >>
              message.request >> mode >> editor >> selection >> modal >> operation >> locked) ||
            !message.processId || !message.sequence || !message.request ||
            !BlenderIdentifier(message.token, 32) || !BlenderIdentifier(message.session, 32) ||
            !BlenderIdentifier(mode, 31) || !BlenderIdentifier(editor, 31) ||
            !BlenderIdentifier(operation, 63) || selection > 7 || modal > 1 || locked > 1 ||
            (!modal && operation != "UNAVAILABLE") || (input >> extra))
        {
            return std::nullopt;
        }
        auto& observation = message.observation;
        observation.connected = true;
        observation.selectionMask = static_cast<std::uint8_t>(selection);
        observation.modalAvailable = modal;
        observation.interfaceLocked = locked;
        std::copy(mode.begin(), mode.end(), observation.mode.begin());
        std::copy(editor.begin(), editor.end(), observation.editor.begin());
        std::copy(operation.begin(), operation.end(), observation.operation.begin());
        return message;
    }
} // namespace Platform
