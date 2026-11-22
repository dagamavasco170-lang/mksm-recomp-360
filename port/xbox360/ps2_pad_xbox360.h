#pragma once

#include <array>
#include <cstdint>

#include "runtime/pad_profile.h"

namespace ps2::xbox360
{
// Button bits use the XInput/XAM INPUT_GAMEPAD layout exposed by the pinned xSDK.
inline constexpr uint16_t PadDpadUp = 0x0001u;
inline constexpr uint16_t PadDpadDown = 0x0002u;
inline constexpr uint16_t PadDpadLeft = 0x0004u;
inline constexpr uint16_t PadDpadRight = 0x0008u;
inline constexpr uint16_t PadStart = 0x0010u;
inline constexpr uint16_t PadBack = 0x0020u;
inline constexpr uint16_t PadLeftThumb = 0x0040u;
inline constexpr uint16_t PadRightThumb = 0x0080u;
inline constexpr uint16_t PadLeftShoulder = 0x0100u;
inline constexpr uint16_t PadRightShoulder = 0x0200u;
inline constexpr uint16_t PadA = 0x1000u;
inline constexpr uint16_t PadB = 0x2000u;
inline constexpr uint16_t PadX = 0x4000u;
inline constexpr uint16_t PadY = 0x8000u;

struct GamepadSnapshot
{
    uint16_t buttons = 0u;
    uint8_t leftTrigger = 0u;
    uint8_t rightTrigger = 0u;
    int16_t leftX = 0;
    int16_t leftY = 0;
    int16_t rightX = 0;
    int16_t rightY = 0;
};

[[nodiscard]] float normalizeStickAxis(int16_t value) noexcept;
[[nodiscard]] ps2::pad::State translateGamepad(const GamepadSnapshot &input) noexcept;
[[nodiscard]] std::array<uint8_t, 32> encodePs2Pad(const GamepadSnapshot &input,
                                                   int player = 0);

} // namespace ps2::xbox360
