#include "ps2_pad_xbox360.h"

#include <cassert>
#include <cstdint>
#include <limits>

int main()
{
    using namespace ps2::xbox360;

    const auto neutral = encodePs2Pad(GamepadSnapshot{});
    assert(neutral[0] == 1u);
    assert(neutral[1] == 0x73u);
    assert((uint16_t(neutral[2]) | (uint16_t(neutral[3]) << 8u)) == 0xFFFFu);
    assert(neutral[4] == 128u && neutral[5] == 128u);
    assert(neutral[6] == 128u && neutral[7] == 128u);

    GamepadSnapshot input{};
    input.connected = true;
    input.buttons = PadBack | PadStart | PadLeftThumb | PadRightThumb |
                    PadLeftShoulder | PadRightShoulder | PadDpadUp | PadDpadRight |
                    PadA | PadB | PadX | PadY;
    input.leftTrigger = 255u;
    input.rightTrigger = 64u;
    input.leftX = std::numeric_limits<int16_t>::max();
    input.leftY = -16384;

    const auto packet = encodePs2Pad(input);
    constexpr uint16_t pressedMask =
        (1u << 0u) | (1u << 1u) | (1u << 2u) | (1u << 3u) |
        (1u << 4u) | (1u << 5u) | (1u << 8u) | (1u << 9u) |
        (1u << 10u) | (1u << 11u) | (1u << 12u) | (1u << 13u) |
        (1u << 14u) | (1u << 15u);
    const uint16_t expectedButtons = static_cast<uint16_t>(0xFFFFu & ~pressedMask);
    assert((uint16_t(packet[2]) | (uint16_t(packet[3]) << 8u)) == expectedButtons);
    assert(packet[6] == 255u);
    assert(packet[7] >= 191u && packet[7] <= 192u);

    assert(normalizeStickAxis(std::numeric_limits<int16_t>::min()) == -1.0f);
    assert(normalizeStickAxis(std::numeric_limits<int16_t>::max()) == 1.0f);
    assert(translateGamepad(input).axes[1] > 0.49f);
    assert(translateGamepad(input).axes[1] < 0.51f);

    GamepadSnapshot disconnected{};
    const auto disconnectedState = translateGamepad(disconnected);
    assert(!disconnectedState.connected);
    const auto disconnectedPacket = encodePs2Pad(disconnected, 1);
    assert((uint16_t(disconnectedPacket[2]) | (uint16_t(disconnectedPacket[3]) << 8u)) == 0xFFFFu);
    return 0;
}
