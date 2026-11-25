#include "ps2_pad_xbox360.h"

#include <algorithm>

namespace ps2::xbox360
{
float normalizeStickAxis(int16_t value) noexcept
{
    constexpr float scale = 1.0f / 32767.0f;
    return std::clamp(static_cast<float>(value) * scale, -1.0f, 1.0f);
}

ps2::pad::State translateGamepad(const GamepadSnapshot &input) noexcept
{
    using namespace ps2::pad;

    State state{};
    state.connected = input.connected;

    state.buttons[South] = (input.buttons & PadA) != 0u;
    state.buttons[East] = (input.buttons & PadB) != 0u;
    state.buttons[West] = (input.buttons & PadX) != 0u;
    state.buttons[North] = (input.buttons & PadY) != 0u;
    state.buttons[Back] = (input.buttons & PadBack) != 0u;
    state.buttons[Start] = (input.buttons & PadStart) != 0u;
    state.buttons[LeftStick] = (input.buttons & PadLeftThumb) != 0u;
    state.buttons[RightStick] = (input.buttons & PadRightThumb) != 0u;
    state.buttons[LeftShoulder] = (input.buttons & PadLeftShoulder) != 0u;
    state.buttons[RightShoulder] = (input.buttons & PadRightShoulder) != 0u;
    state.buttons[Up] = (input.buttons & PadDpadUp) != 0u;
    state.buttons[Right] = (input.buttons & PadDpadRight) != 0u;
    state.buttons[Down] = (input.buttons & PadDpadDown) != 0u;
    state.buttons[Left] = (input.buttons & PadDpadLeft) != 0u;

    state.axes[0] = normalizeStickAxis(input.leftX);
    // The shared pad vocabulary uses positive Y downward; XAM uses positive Y upward.
    state.axes[1] = -normalizeStickAxis(input.leftY);
    state.axes[2] = normalizeStickAxis(input.rightX);
    state.axes[3] = -normalizeStickAxis(input.rightY);
    state.axes[4] = static_cast<float>(input.leftTrigger) / 255.0f;
    state.axes[5] = static_cast<float>(input.rightTrigger) / 255.0f;

    return state;
}

std::array<uint8_t, 32> encodePs2Pad(const GamepadSnapshot &input, int player)
{
    auto profile = ps2::pad::Profile{};
    profile.device = player;
    return ps2::pad::encode(profile, translateGamepad(input));
}
} // namespace ps2::xbox360
