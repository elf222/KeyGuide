// tests/mac_blender_bridge_tests.mm
#include "platform/mac/blender_bridge.hpp"
#include "test_helpers.hpp"

namespace
{
    int wakes = 0;

    void Pump()
    {
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.03, false);
    }
} // namespace

namespace Platform
{
    void WakeMainLoop()
    {
        ++wakes;
    }
} // namespace Platform

int main()
{
    @autoreleasepool
    {
        Platform::Mac::BlenderBridge bridge;
        bridge.initialise(0);
        CHECK(bridge.port() != 0);
        const auto pid = static_cast<std::uint32_t>(getpid());
        bridge.update(pid, true, 10, false);
        const int peer = socket(AF_INET, SOCK_DGRAM, 0);
        CHECK(peer >= 0);
        timeval timeout{1, 0};
        setsockopt(peer, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        sockaddr_in target{};
        target.sin_family = AF_INET;
        target.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        target.sin_port = htons(bridge.port());
        const auto send = [&](const std::string& packet)
        {
            CHECK(sendto(peer, packet.data(), packet.size(), 0,
                         reinterpret_cast<sockaddr*>(&target), sizeof(target)) > 0);
            Pump();
        };
        const auto request = [&]()
        {
            std::array<char, 768> buffer{};
            const auto count = recv(peer, buffer.data(), buffer.size(), 0);
            CHECK(count > 0);
            std::istringstream input{std::string(buffer.data(), count)};
            std::string kind, token;
            unsigned version = 0, process = 0;
            std::uint64_t revision = 0;
            input >> kind >> version >> token >> process >> revision;
            CHECK(kind == "KC_REQUEST" && version == 1 && process == pid);
            return std::pair(token, revision);
        };
        send("KC_HELLO 1 " + std::to_string(pid) + " addon");
        auto [token, revision] = request();
        const auto state_packet = [&](std::uint64_t sequence, std::string_view mode = "EDIT_MESH")
        {
            return "KC_STATE 1 " + token + ' ' + std::to_string(pid) + " addon " +
                   std::to_string(sequence) + ' ' + std::to_string(revision) + ' ' +
                   std::string(mode) + " VIEW_3D 3 1 NONE 0";
        };
        send(state_packet(1));
        CHECK(bridge.read().connected && bridge.read().current &&
              bridge.read().inputSequence == 10);
        CHECK(std::string_view(bridge.read().mode.data()) == "EDIT_MESH");
        const int changed_wakes = wakes;
        send(state_packet(2));
        CHECK(wakes == changed_wakes); // Heartbeat reception never schedules a new GPU frame.
        send(state_packet(1, "OBJECT"));
        CHECK(std::string_view(bridge.read().mode.data()) == "EDIT_MESH");
        bridge.update(pid, true, 11, false);
        CHECK(!bridge.read().current);
        send(state_packet(3, "OBJECT")); // Previous request cannot overwrite the new input epoch.
        CHECK(!bridge.read().current);
        std::tie(token, revision) = request();
        send(state_packet(4, "OBJECT"));
        CHECK(bridge.read().current && bridge.read().inputSequence == 11);
        const auto good_token = token;
        token = "wrong_instance";
        send(state_packet(5, "EDIT_MESH"));
        CHECK(std::string_view(bridge.read().mode.data()) == "OBJECT");
        token = good_token;
        bridge.update(pid, true, 11, false, false, 2, 1);
        CHECK(!bridge.read().current); // Observation loss invalidates a report even without a key.
        std::tie(token, revision) = request();
        send(state_packet(6));
        CHECK(bridge.read().current);
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 4.1, false);
        CHECK(!bridge.read().connected);
        send(state_packet(7));
        CHECK(bridge.read().connected);
        bridge.update(pid, false, 12, false);
        CHECK(!bridge.read().connected);
        bridge.shutdown();
        CHECK(bridge.port() == 0);
        close(peer);
        std::cout << "Local Blender IPC / ordering / heartbeat / timeout checks passed\n";
    }
}
