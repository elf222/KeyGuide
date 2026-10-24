// platform/mac/blender_bridge.hpp
#pragma once
#include <array>
#include <chrono>
#include <sstream>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#import <Cocoa/Cocoa.h>
#include "platform/blender_protocol.hpp"
#include "platform/mac/mac_bridge.hpp"

namespace Platform::Mac
{
    // Header implementation keeps this backend in the existing macOS compilation unit.
    // No native types or socket APIs enter input.cpp or the portable command pipeline.
    class BlenderBridge
    {
        using Clock = std::chrono::steady_clock;
        CFSocketRef socket = nullptr;
        CFRunLoopSourceRef source = nullptr;
        sockaddr_in peer{};
        bool havePeer = false, enabled = false;
        std::uint32_t processId = 0;
        std::uint64_t request = 1, lastSequence = 0, receivedRequest = 0, inputSequence = 0;
        std::uint64_t foregroundGeneration = 0, lossGeneration = 0;
        std::string token, session;
        BlenderObservation observation{};
        Clock::time_point receivedAt{}, sentAt{};
        std::array<double, 4> pointerWindow{};
        std::vector<CGRect> windowBounds;

        void sendRequest()
        {
            if (!socket || !havePeer)
            {
                return;
            }
            std::ostringstream packet;
            packet.precision(12);
            packet << "KC_REQUEST 1 " << token << ' ' << processId << ' ' << request << ' '
                   << enabled << ' ' << pointerWindow[0] << ' ' << pointerWindow[1] << ' '
                   << pointerWindow[2] << ' ' << pointerWindow[3];
            const auto text = packet.str();
            sendto(CFSocketGetNative(socket), text.data(), text.size(), 0,
                   reinterpret_cast<const sockaddr*>(&peer), sizeof(peer));
            sentAt = Clock::now();
        }

        void receive()
        {
            std::array<char, BLENDER_PACKET_LIMIT + 1> buffer{};
            for (int budget = 0; budget < 16; ++budget)
            {
                sockaddr_in sender{};
                socklen_t length = sizeof(sender);
                const auto count = recvfrom(CFSocketGetNative(socket), buffer.data(), buffer.size(),
                                            0, reinterpret_cast<sockaddr*>(&sender), &length);
                if (count <= 0)
                {
                    break;
                }
                if (sender.sin_addr.s_addr != htonl(INADDR_LOOPBACK))
                {
                    continue;
                }
                const auto message =
                    ParseBlenderMessage({buffer.data(), static_cast<std::size_t>(count)});
                if (!message || message->processId != processId || !processId)
                {
                    continue;
                }
                if (message->hello)
                {
                    const bool new_peer = !havePeer || session != message->session ||
                                          peer.sin_port != sender.sin_port;
                    if (new_peer)
                    {
                        observation = {};
                        lastSequence = receivedRequest = 0;
                    }
                    peer = sender;
                    session = message->session;
                    havePeer = true;
                    sendRequest();
                    continue;
                }
                if (!enabled || !havePeer || sender.sin_port != peer.sin_port ||
                    message->token != token || message->session != session ||
                    message->sequence <= lastSequence || message->request != request)
                {
                    continue;
                }
                auto next = message->observation;
                next.current = true;
                next.inputSequence = inputSequence;
                const bool changed = next != observation;
                observation = next;
                lastSequence = message->sequence;
                receivedRequest = message->request;
                receivedAt = Clock::now();
                if (changed)
                {
                    WakeMainLoop();
                }
            }
        }

        void updatePointer(bool refresh_geometry)
        {
            const auto mouse = [NSEvent mouseLocation];
            const auto screens = [NSScreen screens];
            if (![screens count])
            {
                return;
            }
            const double desktop_top = [[screens objectAtIndex:0] frame].size.height;
            if (refresh_geometry || windowBounds.empty())
            {
                windowBounds.clear();
                NSArray* windows = CFBridgingRelease(CGWindowListCopyWindowInfo(
                    kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements,
                    kCGNullWindowID));
                for (NSDictionary* window in windows)
                {
                    if ([window[(id)kCGWindowOwnerPID] unsignedIntValue] != processId ||
                        [window[(id)kCGWindowLayer] intValue] != 0)
                    {
                        continue;
                    }
                    CGRect bounds{};
                    if (CGRectMakeWithDictionaryRepresentation(
                            (__bridge CFDictionaryRef)window[(id)kCGWindowBounds], &bounds) &&
                        bounds.size.width >= 100 && bounds.size.height >= 100)
                    {
                        windowBounds.push_back(bounds);
                    }
                }
            }
            for (const auto& bounds : windowBounds)
            {
                const double x = mouse.x - bounds.origin.x;
                const double y = mouse.y - (desktop_top - CGRectGetMaxY(bounds));
                if (x >= 0 && y >= 0 && x < bounds.size.width && y < bounds.size.height)
                {
                    pointerWindow = {x, y, bounds.size.width, bounds.size.height};
                    return;
                }
            }
            // Hovering the cheat sheet retains the last Blender editor for readable hints.
        }

      public:
        void initialise(unsigned short port = BLENDER_BRIDGE_PORT)
        {
            const int descriptor = ::socket(AF_INET, SOCK_DGRAM, 0);
            if (descriptor < 0)
            {
                return;
            }
            sockaddr_in address{};
            address.sin_family = AF_INET;
            address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            address.sin_port = htons(port);
            if (bind(descriptor, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0 ||
                fcntl(descriptor, F_SETFL, O_NONBLOCK) < 0)
            {
                close(descriptor);
                return;
            }
            token = [[[NSUUID UUID] UUIDString] stringByReplacingOccurrencesOfString:@"-"
                                                                          withString:@""]
                        .UTF8String;
            CFSocketContext context{0, this, nullptr, nullptr, nullptr};
            socket = CFSocketCreateWithNative(
                kCFAllocatorDefault, descriptor, kCFSocketReadCallBack,
                [](CFSocketRef, CFSocketCallBackType, CFDataRef, const void*, void* info)
                {
                    static_cast<BlenderBridge*>(info)->receive();
                },
                &context);
            if (!socket)
            {
                close(descriptor);
                return;
            }
            source = CFSocketCreateRunLoopSource(kCFAllocatorDefault, socket, 0);
            if (!source)
            {
                shutdown();
                return;
            }
            CFRunLoopAddSource(CFRunLoopGetMain(), source, kCFRunLoopCommonModes);
        }

        unsigned short port() const
        {
            if (!socket)
            {
                return 0;
            }
            sockaddr_in address{};
            socklen_t size = sizeof(address);
            if (getsockname(CFSocketGetNative(socket), reinterpret_cast<sockaddr*>(&address),
                            &size) != 0)
            {
                return 0;
            }
            return ntohs(address.sin_port);
        }

        void shutdown()
        {
            enabled = false;
            ++request;
            sendRequest();
            if (source)
            {
                CFRunLoopRemoveSource(CFRunLoopGetMain(), source, kCFRunLoopCommonModes);
                CFRelease(source);
                source = nullptr;
            }
            if (socket)
            {
                CFSocketInvalidate(socket);
                CFRelease(socket);
                socket = nullptr;
            }
            havePeer = false;
            observation = {};
        }

        void update(std::uint32_t pid, bool active, std::uint64_t sequence, bool pointer_changed,
                    bool refresh_geometry = false, std::uint64_t foreground = 0,
                    std::uint64_t loss = 0)
        {
            if (!active)
            {
                sequence = 0;
            }
            bool changed = pid != processId || active != enabled || sequence != inputSequence ||
                           foreground != foregroundGeneration || loss != lossGeneration;
            foregroundGeneration = foreground;
            lossGeneration = loss;
            if (pid != processId)
            {
                processId = pid;
                havePeer = false;
                observation = {};
                pointerWindow = {};
                windowBounds.clear();
                lastSequence = receivedRequest = 0;
            }
            enabled = active;
            inputSequence = sequence;
            bool pointer_moved = false;
            if (active && (changed || pointer_changed))
            {
                const auto previous = pointerWindow;
                updatePointer(refresh_geometry);
                pointer_moved = previous != pointerWindow;
            }
            if (changed)
            {
                ++request;
            }
            if (changed || pointer_moved || Clock::now() - sentAt >= std::chrono::seconds(2))
            {
                sendRequest();
            }
        }

        BlenderObservation read() const
        {
            auto result = observation;
            if (!enabled || Clock::now() - receivedAt > std::chrono::seconds(4))
            {
                return {};
            }
            result.current = receivedRequest == request;
            return result;
        }
    };
} // namespace Platform::Mac
