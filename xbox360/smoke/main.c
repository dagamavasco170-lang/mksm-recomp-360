// Xbox 360 bring-up smoke test. Adapted from xsdk's tested template.
#include <stdbool.h>
#include <stdint.h>

#include "xsdk/gpu.h"
#include "xsdk/kernel.h"
#include "xsdk/log.h"
#include "xsdk/present.h"
#include "xsdk/splash.h"
#include "xsdk/texture.h"

enum {
    SPLASH_MS = 1500,
    FRAME_BUFFERS = 2,
    SWAP_INTERVAL = 1,
    BEGIN_TIMEOUT_MS = 5000,
    FENCE_TIMEOUT_MS = 1000,
    CLEAR_ARGB = 0xFF203060,
    MAX_WIDTH = 1280,
};

static uint32_t passed_checks;
static uint32_t failed_checks;

static void check(const char *name, bool passed)
{
    xsdk_log_write("%s: %s", passed ? "PASS" : "FAIL", name);
    passed_checks += passed;
    failed_checks += !passed;
}

static uint32_t frame_pixel(uint32_t address, uint32_t width, uint32_t y)
{
    XsdkTexture frame;
    if (width > MAX_WIDTH ||
        !NT_SUCCESS(xsdk_texture_init(&frame, XSDK_FORMAT_A8R8G8B8, width, height, 1, true)))
        return 0;

    xsdk_texture_attach(&frame, xsdk_present_pixels(address));
    static uint32_t row[MAX_WIDTH];
    xsdk_texture_download_rows(&frame, 0, y, 1, row, sizeof row);
    return __builtin_bswap32(row[width / 2]);
}

static bool check_first_frame(uint32_t address, uint32_t width)
{
    NTSTATUS status = xsdk_gpu_wait(xsdk_gpu_fence(), FENCE_TIMEOUT_MS);
    if (!NT_SUCCESS(status))
        xsdk_log_write("smoke: GPU fence wait failed: 0x%08X", (unsigned)status);

    uint32_t pixel = frame_pixel(address, width, height, height / 2);
    xsdk_log_write("smoke: center pixel=0x%08X expected=0x%08X",
                   (unsigned)pixel, (unsigned)CLEAR_ARGB);
    bool ok = NT_SUCCESS(status) && pixel == CLEAR_ARGB;
    check("first resolved frame contains the clear colour", ok);
    return ok;
}

int main(void)
{
    xsdk_log_write("mksm-xbox360-smoke: start; press Back to exit");

    NTSTATUS status = xsdk_splash_show(SPLASH_MS);
    if (!NT_SUCCESS(status))
        xsdk_log_write("smoke: splash failed (non-fatal): 0x%08X", (unsigned)status);

    status = xsdk_gpu_open();
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

    xsdk_log_write("smoke: display ready at %ux%u", (unsigned)width, (unsigned)height);
    check("display dimensions are non-zero", width > 0 && height > 0);

    uint32_t frames = 0;
    bool first_frame_checked = false;
    bool loop_ok = true;
    for (;;) {
        INPUT_STATE input = {0};
        if (XamInputGetState(0, 0, &input) == ERROR_SUCCESS &&
            (input.gamepad.buttons & INPUT_GAMEPAD_BACK))
            break;

        uint32_t address = 0;
        status = xsdk_present_begin(&address, BEGIN_TIMEOUT_MS);
        if (!NT_SUCCESS(status)) {
            xsdk_log_write("smoke: xsdk_present_begin failed: 0x%08X", (unsigned)status);
            loop_ok = false;
            break;
        }

        xsdk_gpu_resolve(address, width, height, CLEAR_ARGB);
        if (frames == 1) {
            first_frame_checked = check_first_frame(address, width, height);
        }
        xsdk_present_end();
        ++frames;
    }

    xsdk_present_close();
    status = xsdk_gpu_close();
    check("GPU closed cleanly", NT_SUCCESS(status));
    check("at least one frame presented", frames > 0);
    check("frame loop did not fail", loop_ok);
    check("clear-colour pixel verification ran", first_frame_checked);

    xsdk_log_write("smoke: frames=%u checks=%u passed, %u failed",
                   (unsigned)frames, (unsigned)passed_checks, (unsigned)failed_checks);
    xsdk_log_write("mksm-xbox360-smoke: %s", failed_checks == 0 ? "PASS" : "FAIL");
    return failed_checks == 0 ? 0 : 3;
}
