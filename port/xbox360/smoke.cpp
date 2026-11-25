#include <algorithm>
#include <array>
#include <bit>
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

#include "ps2_guest_memory.h"
#include "ps2_pad_xbox360.h"
#include "ps2_ee_test_program.h"
#include "ps2_simd_scalar.h"
#include "ps2_spr_dma.h"
#include "runtime/ps2_pad.h"

#if defined(MKSM_HAS_GENERATED_RECOMP_PROBE)
bool checkGeneratedRecompProbe();
#endif

static_assert(sizeof(void *) == 4u, "Xbox 360 target must use a 32-bit pointer ABI");
static_assert(std::endian::native == std::endian::big,
              "Xbox 360 target must use the big-endian PowerPC ABI");

namespace
{
constexpr uint32_t kClearArgb = 0xFF182840u;
constexpr uint32_t kBuffers = 2u;
constexpr uint32_t kSwapInterval = 1u;
constexpr uint32_t kBeginTimeoutMs = 5000u;

bool checkGuestMemoryEndian()
{
    std::array<uint8_t, 16> probe{};
    for (size_t i = 0; i < probe.size(); ++i)
        probe[i] = static_cast<uint8_t>(i);

    uint32_t decoded = 0u;
    return ps2::xbox360::guest_memory::readLittleEndian(
               std::span<const uint8_t>(probe), 14u, decoded) &&
           decoded == 0x01000F0Eu;
}

bool checkScalarMmi()
{
    using namespace ps2::xbox360::simd;
    Vector128 lhs{};
    Vector128 rhs{};
    writeLane<uint32_t>(lhs, 0u, 0xFFFFFFFFu);
    writeLane<uint32_t>(rhs, 0u, 1u);
    const auto sum = addLanes<uint32_t>(lhs, rhs);
    const auto unpack = interleaveLow<uint32_t>(lhs, rhs);
    Vector128 shiftCounts{};
    writeLane<uint32_t>(shiftCounts, 0u, 1u);
    const auto shifted = shiftRightLogicalVariable32(lhs, shiftCounts);
    Vector128 saturationLhs{};
    Vector128 saturationRhs{};
    writeLane<uint32_t>(saturationLhs, 0u, 0x7FFFFFFFu);
    writeLane<uint32_t>(saturationRhs, 0u, 1u);
    const auto saturated = addSaturatingSigned32(saturationLhs, saturationRhs);
    Vector128 signedByteA{}, signedByteB{};
    writeLane<uint8_t>(signedByteA, 0u, 0x7Fu);
    writeLane<uint8_t>(signedByteB, 0u, 1u);
    const auto saturatedByte = addSaturatingSignedLanes<uint8_t>(signedByteA, signedByteB);
    Vector128 unsignedByteA{}, unsignedByteB{};
    writeLane<uint8_t>(unsignedByteA, 0u, 0xFFu);
    writeLane<uint8_t>(unsignedByteB, 0u, 1u);
    const auto saturatedUnsignedByte = addSaturatingUnsignedLanes<uint8_t>(unsignedByteA, unsignedByteB);
    const auto unsignedWordSum = addSaturatingUnsignedLanes<uint32_t>(lhs, rhs);
    const auto unsignedWordDifference = subtractSaturatingUnsignedLanes<uint32_t>(rhs, lhs);
    Vector128 maddLhs{}, maddRhs{}, maddHi{}, maddLo{};
    for (size_t i = 0; i < 8u; i += 2u)
    {
        writeLane<uint16_t>(maddLhs, i, static_cast<uint16_t>(i / 2u + 1u));
        writeLane<uint16_t>(maddRhs, i, 1u);
    }
    const auto madd = pmaddh(maddLhs, maddRhs, maddHi, maddLo);
    const auto packed = packEvenLanes<uint32_t>(lhs, rhs);
    return readLane<uint32_t>(sum, 0u) == 0u &&
           readLane<uint32_t>(unpack, 0u) == 1u &&
           readLane<uint32_t>(unpack, 1u) == 0xFFFFFFFFu &&
           readLane<uint32_t>(shifted, 0u) == 0x7FFFFFFFu &&
           readLane<uint32_t>(saturated, 0u) == 0x7FFFFFFFu &&
           readLane<uint8_t>(saturatedByte, 0u) == 0x7Fu &&
           readLane<uint8_t>(saturatedUnsignedByte, 0u) == 0xFFu &&
           readLane<uint32_t>(unsignedWordSum, 0u) == 0xFFFFFFFFu &&
           readLane<uint32_t>(unsignedWordDifference, 0u) == 0u &&
           readLane<uint32_t>(madd.destination, 0u) == 1u &&
           readLane<uint32_t>(madd.destination, 1u) == 2u &&
           readLane<uint32_t>(madd.destination, 2u) == 3u &&
           readLane<uint32_t>(madd.destination, 3u) == 4u &&
           readLane<uint32_t>(madd.lo, 0u) == 1u &&
           readLane<uint32_t>(madd.hi, 0u) == 2u &&
           readLane<uint32_t>(packed, 0u) == 1u;
}

bool checkDeterministicEeTrace()
{
    const auto trace = ps2::xbox360::ee::test_program::runDeterministicTrace();
    if (!trace.passed)
    {
        xsdk_log_write("MKSM smoke: EE deterministic trace FAILED mask=%08X pc=%08X retired=%u",
                       static_cast<unsigned>(trace.failureMask),
                       static_cast<unsigned>(trace.pc),
                       static_cast<unsigned>(trace.retiredInstructions));
        return false;
    }

    xsdk_log_write("MKSM_EE_TRACE signature=%08X%08X failures=%08X pc=%08X retired=%u",
                   static_cast<unsigned>(trace.signature >> 32u),
                   static_cast<unsigned>(trace.signature),
                   static_cast<unsigned>(trace.failureMask),
                   static_cast<unsigned>(trace.pc),
                   static_cast<unsigned>(trace.retiredInstructions));
    return true;
}

bool checkScratchpadDma()
{
    using namespace ps2::xbox360::dmac;
    std::array<uint8_t,32> ram{};
    std::array<uint8_t,kScratchpadSize> scratch{};
    ControllerState controller{};
    constexpr uint32_t start=0x3FF0u;
    for(uint32_t i=0;i<32u;++i)scratch[(start+i)&0x3FFFu]=static_cast<uint8_t>(0x20u+i);
    const std::span<uint8_t> ramView(ram),scratchView(scratch);
    const auto write=[&](uint32_t a,uint32_t v){return writeRegister(controller,ramView,scratchView,a,v);};
    const auto read=[&](uint32_t a,uint32_t &v){return readRegister(controller,a,v);};
    bool ok=write(kControllerControl,0x21u).status==WriteStatus::Stored&&
        write(kScratchpadToRdramChannel+0x10u,0u).status==WriteStatus::Stored&&
        write(kScratchpadToRdramChannel+0x20u,2u).status==WriteStatus::Stored&&
        write(kScratchpadToRdramChannel+0x80u,start).status==WriteStatus::Stored;
    const auto fromSpr=write(kScratchpadToRdramChannel,kStart);
    ok=ok&&fromSpr.status==WriteStatus::TransferCompleted&&fromSpr.bytesTransferred==32u;
    for(uint32_t i=0;i<32u;++i)ok=ok&&ram[i]==static_cast<uint8_t>(0x20u+i);
    uint32_t madr=0u,qwc=1u,sadr=1u,chcr=1u,stadr=0u,dstat=0u;
    ok=ok&&read(kScratchpadToRdramChannel+0x10u,madr)&&madr==32u&&
        read(kScratchpadToRdramChannel+0x20u,qwc)&&qwc==0u&&
        read(kScratchpadToRdramChannel+0x80u,sadr)&&sadr==0x10u&&
        read(kScratchpadToRdramChannel,chcr)&&chcr==0u&&
        read(kStallAddress,stadr)&&stadr==32u&&
        read(kControllerStatus,dstat)&&(dstat&(1u<<8u))!=0u;
    for(uint32_t i=0;i<32u;++i)ram[i]=static_cast<uint8_t>(0xA0u+i);
    ok=ok&&write(kRdramToScratchpadChannel+0x10u,0u).status==WriteStatus::Stored&&
        write(kRdramToScratchpadChannel+0x20u,2u).status==WriteStatus::Stored&&
        write(kRdramToScratchpadChannel+0x80u,start).status==WriteStatus::Stored;
    const auto toSpr=write(kRdramToScratchpadChannel,kStart);
    ok=ok&&toSpr.status==WriteStatus::TransferCompleted;
    for(uint32_t i=0;i<32u;++i)ok=ok&&scratch[(start+i)&0x3FFFu]==static_cast<uint8_t>(0xA0u+i);
    ok=ok&&read(kControllerStatus,dstat)&&(dstat&(1u<<9u))!=0u;
    xsdk_log_write("MKSM_SPR_DMA_TRACE failures=%08X bytes=%u causes=%u",ok?0u:1u,32u,static_cast<unsigned>(dstat&0x300u));
    return ok;
}

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
           !normalizedPath.empty() &&
           ps2::xbox360::encodePs2Pad(ps2::xbox360::GamepadSnapshot{})[1] == 0x73u;
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
    xsdk_log_write("MKSM smoke: XAM-to-PS2 controller adapter ready (player 1)");

    PSPadBackend ps2PadBackend;
    std::array<uint8_t, 32> ps2PadReport{};

    uint32_t presentedFrames = 0u;
    int exitCode = 0;
    bool backPressed = false;

    for (;;)
    {
        if (!ps2PadBackend.readState(0, 0, ps2PadReport.data(), ps2PadReport.size()))
        {
            xsdk_log_write("MKSM smoke: PS2 pad adapter rejected player 1 report");
            exitCode = 1;
            break;
        }

        const uint16_t ps2Buttons =
            static_cast<uint16_t>(ps2PadReport[2]) |
            (static_cast<uint16_t>(ps2PadReport[3]) << 8u);
        // The shared PS2 pad profile maps Xbox Back to the PS2 Select bit (active low).
        if ((ps2Buttons & 0x0001u) == 0u)
        {
            backPressed = true;
            xsdk_log_write("MKSM smoke: Xbox Back received as PS2 Select");
            break;
        }

        uint32_t backBufferAddress = 0u;
        status = xsdk_present_begin(&backBufferAddress, kBeginTimeoutMs);
        if (!NT_SUCCESS(status))
        {
            xsdk_log_write("MKSM smoke: present_begin failed: 0x%08X", static_cast<unsigned>(status));
            exitCode = 1;
            break;
        }

        xsdk_gpu_resolve(backBufferAddress, width, height, kClearArgb);
        xsdk_present_end();
        ++presentedFrames;
    }

    xsdk_present_close();
    status = xsdk_gpu_close();
    if (!NT_SUCCESS(status))
    {
        xsdk_log_write("MKSM smoke: xsdk_gpu_close failed: 0x%08X", static_cast<unsigned>(status));
        exitCode = 1;
    }

    xsdk_log_write("MKSM smoke: presented %u frames; shutdown=%s; exit=%d",
                   static_cast<unsigned>(presentedFrames),
                   backPressed ? "Back" : "error",
                   exitCode);
    return exitCode;
}
} // namespace

int main()
{
    xsdk_log_write("MKSM Xbox 360 smoke: starting");
    xsdk_log_write("MKSM smoke: target ABI verified (32-bit big-endian PowerPC)");

    if (!checkCpp20Runtime())
    {
        xsdk_log_write("MKSM smoke: C++20 runtime checks failed");
        return 2;
    }

    if (!checkGuestMemoryEndian())
    {
        xsdk_log_write("MKSM smoke: little-endian guest memory check failed");
        return 3;
    }

    if (!checkDeterministicEeTrace())
    {
        xsdk_log_write("MKSM smoke: deterministic EE program check failed");
        return 5;
    }

    if (!checkScalarMmi())
    {
        xsdk_log_write("MKSM smoke: scalar PS2 packed-vector check failed");
        return 4;
    }
    if (!checkScratchpadDma())
    {
        xsdk_log_write("MKSM smoke: scratchpad DMA checks failed");
        return 7;
    }

#if defined(MKSM_HAS_GENERATED_RECOMP_PROBE)
    if (!checkGeneratedRecompProbe())
    {
        xsdk_log_write("MKSM smoke: PS2Recomp-generated Xenon probe failed");
        return 6;
    }
#endif

    xsdk_log_write("MKSM smoke: C++20 runtime checks passed");
    xsdk_log_write("MKSM smoke: little-endian guest memory check passed");
    xsdk_log_write("MKSM smoke: scalar PS2 packed-vector checks passed");
    return runTitle();
}
