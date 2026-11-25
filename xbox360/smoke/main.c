#include <stdint.h>
#include "xsdk/gpu.h"
#include "xsdk/kernel.h"
#include "xsdk/log.h"
#include "xsdk/present.h"

enum {
    FRAME_BUFFERS = 2,
    SWAP_INTERVAL = 1,
    FRAME_TIMEOUT_MS = 5000,
    CLEAR_COLOR = 0xFF203060,
};

int main(void)
{
    xsdk_log_write("mksm-xbox360-smoke: start; press Back to exit");

    NTSTATUS status = xsdk_gpu_open();
    if (!NT_SUCCESS(status)) {
        xsdk_log_write("smoke: xsdk_gpu_open failed: 0x%08X", (unsigned)status);
        return 1;
    }

    uint32_t width = 0;
    uint32_t height = 0;
    status = xsdk_present_open(XSDK_DISPLAY_720P, FRAME_BUFFERS, SWAP_INTERVAL, &width, &height);
    if (!NT_SUCCESS(status)) {
        xsdk_log_write("smoke: xsdk_present_open failed: 0x%08X", (unsigned)status);
        xsdk_gpu_close();
        return 2;
    }

    xsdk_log_write("smoke: presentation opened at %ux%u", (unsigned)width, (unsigned)height);

    uint32_t frames = 0;
    for (;;) {
        INPUT_STATE input = {0};
        if (XamInputGetState(0, 0, &input) == ERROR_SUCCESS &&
            (input.gamepad.buttons & INPUT_GAMEPAD_BACK))
            break;

        uint32_t address = 0;
        status = xsdk_present_begin(&address, FRAME_TIMEOUT_MS);
        if (!NT_SUCCESS(status)) {
            xsdk_log_write("smoke: present_begin failed: 0x%08X", (unsigned)status);
            break;
        }

        xsdk_gpu_resolve(address, width, height, CLEAR_COLOR);
        xsdk_present_end();
        ++frames;
    }

    xsdk_present_close();
    status = xsdk_gpu_close();
    xsdk_log_write("smoke: frames=%u; gpu_close=0x%08X", (unsigned)frames, (unsigned)status);
    xsdk_log_write("mksm-xbox360-smoke: end");
    return NT_SUCCESS(status) ? 0 : 3;
}
