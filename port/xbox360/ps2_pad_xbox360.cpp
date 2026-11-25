#include "ps2_pad_xbox360.h"

#include <algorithm>

#include "runtime/ps2_pad.h"
#include "xsdk/kernel.h"

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
        snapshot.connected = true;
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
