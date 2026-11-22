#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <ranges>
#include <span>
#include <thread>
#include <vector>

#include "xsdk/gpu.h"
#include "xsdk/kernel.h"
#include "xsdk/log.h"
#include "xsdk/present.h"
#include "xsdk/splash.h"

namespace
{
constexpr uint32_t kClearArgb = 0xFF182840u;
constexpr uint32_t kBuffers = 2u;
constexpr uint32_t kSwapInterval = 1u;
constexpr uint32_t kBeginTimeoutMs = 5000u;

bool checkCpp20Runtime()
{
    std::vector<uint32_t> values{3u, 1u, 2u};
    std::ranges::sort(values);

    std::array<uint32_t, 3> copied{};
    const std::span<uint32_t> view(copied);
    std::ranges::copy(values, view.begin());

    std::atomic<std::shared_ptr<uint32_t>> shared;
    shared.store(std::make_shared<uint32_t>(view[1]), std::memory_order_release);

    std::atomic<uint32_t> workerValue{0u};
    std::thread worker([&workerValue]
                       { workerValue.store(0x360u, std::memory_order_release); });
    worker.join();

    const std::filesystem::path gamePath("game:/MortalKombatShaolinMonks");
    const auto normalizedPath = gamePath.lexically_normal();
    const auto elapsed = std::chrono::steady_clock::now().time_since_epoch();
    const auto published = shared.load(std::memory_order_acquire);
    (void)elapsed;

    return values[0] == 1u &&
           published &&
           *published == 2u &&
           workerValue.load(std::memory_order_acquire) == 0x360u &&
           !normalizedPath.empty();
}

int runTitle()
{
    const NTSTATUS splash = xsdk_splash_show(1200u);
    if (!NT_SUCCESS(splash))
        xsdk_log_write("MKSM smoke: splash returned 0x%08X", static_cast<unsigned>(splash));

    NTSTATUS status = xsdk_gpu_open();
    if (!NT_SUCCESS(status))
    {
        xsdk_log_write("MKSM smoke: xsdk_gpu_open failed: 0x%08X", static_cast<unsigned>(status));
        return 1;
    }

    uint32_t width = 0u;
    uint32_t height = 0u;
    status = xsdk_present_open(XSDK_DISPLAY_720P, kBuffers, kSwapInterval, &width, &height);
    if (!NT_SUCCESS(status))
    {
        xsdk_log_write("MKSM smoke: xsdk_present_open failed: 0x%08X", static_cast<unsigned>(status));
        xsdk_gpu_close();
        return 1;
    }

    xsdk_log_write("MKSM smoke: display opened at %ux%u", static_cast<unsigned>(width), static_cast<unsigned>(height));

    for (;;)
    {
        INPUT_STATE input{};
        if (XamInputGetState(0u, 0u, &input) == ERROR_SUCCESS &&
            (input.gamepad.buttons & INPUT_GAMEPAD_BACK) != 0u)
            break;

        uint32_t backBufferAddress = 0u;
        status = xsdk_present_begin(&backBufferAddress, kBeginTimeoutMs);
        if (!NT_SUCCESS(status))
        {
            xsdk_log_write("MKSM smoke: present_begin failed: 0x%08X", static_cast<unsigned>(status));
            break;
        }

        xsdk_gpu_resolve(backBufferAddress, width, height, kClearArgb);
        xsdk_present_end();
    }

    xsdk_present_close();
    status = xsdk_gpu_close();
    if (!NT_SUCCESS(status))
        xsdk_log_write("MKSM smoke: xsdk_gpu_close failed: 0x%08X", static_cast<unsigned>(status));

    return 0;
}
} // namespace

int main()
{
    xsdk_log_write("MKSM Xbox 360 smoke: starting");

    if (!checkCpp20Runtime())
    {
        xsdk_log_write("MKSM smoke: C++20 runtime checks failed");
        return 2;
    }

    xsdk_log_write("MKSM smoke: C++20 runtime checks passed");
    return runTitle();
}
