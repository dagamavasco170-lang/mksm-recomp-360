#include "ps2_pad_xbox360.h"

#include <algorithm>
#include <cmath>

#include "runtime/ps2_pad.h"
#include "xsdk/kernel.h"

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
    state.connected = true;

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

static_assert(INPUT_GAMEPAD_DPAD_UP == ps2::xbox360::PadDpadUp);
static_assert(INPUT_GAMEPAD_DPAD_DOWN == ps2::xbox360::PadDpadDown);
static_assert(INPUT_GAMEPAD_DPAD_LEFT == ps2::xbox360::PadDpadLeft);
static_assert(INPUT_GAMEPAD_DPAD_RIGHT == ps2::xbox360::PadDpadRight);
static_assert(INPUT_GAMEPAD_START == ps2::xbox360::PadStart);
static_assert(INPUT_GAMEPAD_BACK == ps2::xbox360::PadBack);
static_assert(INPUT_GAMEPAD_A == ps2::xbox360::PadA);
static_assert(INPUT_GAMEPAD_B == ps2::xbox360::PadB);
static_assert(INPUT_GAMEPAD_X == ps2::xbox360::PadX);
static_assert(INPUT_GAMEPAD_Y == ps2::xbox360::PadY);

bool PSPadBackend::readState(int port, int slot, uint8_t *data, size_t size)
{
    if (!data || size < 32u || port < 0 || port > 1 || slot != 0)
        return false;

    INPUT_STATE input{};
    ps2::xbox360::GamepadSnapshot snapshot{};

    const uint32_t result = XamInputGetState(static_cast<uint32_t>(port), 0u, &input);
    if (result == ERROR_SUCCESS)
    {
        snapshot.buttons = input.gamepad.buttons;
        snapshot.leftTrigger = input.gamepad.left_trigger;
        snapshot.rightTrigger = input.gamepad.right_trigger;
        snapshot.leftX = input.gamepad.thumb_lx;
        snapshot.leftY = input.gamepad.thumb_ly;
        snapshot.rightX = input.gamepad.thumb_rx;
        snapshot.rightY = input.gamepad.thumb_ry;
    }

    const auto packet = ps2::xbox360::encodePs2Pad(snapshot, port);
    std::copy(packet.begin(), packet.end(), data);
    return true;
}
