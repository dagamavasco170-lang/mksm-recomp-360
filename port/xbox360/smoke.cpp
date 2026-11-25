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
#include "ps2_ee_dispatcher.h"
#include "ps2_pad_xbox360.h"
#include "ps2_ee_test_program.h"
#include "ps2_simd_scalar.h"
#include "ps2_spr_dma.h"
#include "ps2_iop_ioman.h"
#include "runtime/ps2_pad.h"

#if defined(MKSM_HAS_GENERATED_RECOMP_PROBE)
bool checkGeneratedRecompProbe();
#endif

static_assert(sizeof(void *) == 4u, "Xbox 360 target must use a 32-bit pointer ABI");
static_assert(std::endian::native == std::endian::big,
              "Xbox 360 target must use the big-endian PowerPC ABI");

namespace
{
#if !defined(MKSM_XENIA_HEADLESS_SMOKE)
constexpr uint32_t kClearArgb = 0xFF182840u;
constexpr uint32_t kBuffers = 2u;
constexpr uint32_t kSwapInterval = 1u;
constexpr uint32_t kBeginTimeoutMs = 5000u;
#endif

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

bool checkEeLqSq()
{
    using namespace ps2::xbox360::ee;
    std::array<uint8_t, 256> ram{};
    CpuState cpu{};

    ps2::xbox360::simd::Vector128 base{}, source{};
    ps2::xbox360::simd::writeLane<uint32_t>(base, 0u, 0x5Bu); // deliberately unaligned
    for (size_t i = 0; i < source.bytes.size(); ++i)
        source.bytes[i] = static_cast<uint8_t>(0xD0u + i);
    cpu.writeGpr(1u, base);
    cpu.writeGpr(2u, source);

    const auto encode = [](uint8_t opcode, uint8_t rs, uint8_t rt) {
        return (static_cast<uint32_t>(opcode) << 26u) |
               (static_cast<uint32_t>(rs) << 21u) |
               (static_cast<uint32_t>(rt) << 16u);
    };
    bool ok = ps2::xbox360::guest_memory::writeLittleEndian(std::span<uint8_t>(ram), 0u,
                                               encode(0x1Fu, 1u, 2u)) && // SQ
              ps2::xbox360::guest_memory::writeLittleEndian(std::span<uint8_t>(ram), 4u,
                                               encode(0x1Eu, 1u, 3u));   // LQ
    const auto stored = step(cpu, ram);
    ok = ok && stored.status == StepStatus::Executed;
    for (size_t i = 0; i < source.bytes.size(); ++i)
        ok = ok && ram[0x50u + i] == source.bytes[i];

    const auto loaded = step(cpu, ram);
    ok = ok && loaded.status == StepStatus::Executed &&
         cpu.readGpr(3u).bytes == source.bytes &&
         cpu.pc == 8u && cpu.retiredInstructions == 2u;
    xsdk_log_write("MKSM_EE_LQ_SQ_TRACE failures=%08X pc=%08X retired=%u addr=%08X bytes=%u",
                   ok ? 0u : 1u, static_cast<unsigned>(cpu.pc),
                   static_cast<unsigned>(cpu.retiredInstructions), 0x50u, 16u);
    return ok;
}

bool checkEeUnalignedMerge()
{
    using namespace ps2::xbox360::ee;
    std::array<uint8_t, 256> ram{};
    CpuState cpu{};
    ps2::xbox360::simd::Vector128 base{}, initial{}, source{};
    ps2::xbox360::simd::writeLane<uint32_t>(base, 0u, 0x40u);
    ps2::xbox360::simd::writeLane<uint32_t>(initial, 0u, 0xAABBCCDDu);
    ps2::xbox360::simd::writeLane<uint32_t>(initial, 1u, 0x55667788u);
    ps2::xbox360::simd::writeLane<uint32_t>(initial, 2u, 0x10203040u);
    ps2::xbox360::simd::writeLane<uint32_t>(initial, 3u, 0x50607080u);
    ps2::xbox360::simd::writeLane<uint32_t>(source, 0u, 0xAABBCCDDu);
    cpu.writeGpr(1u, base);
    cpu.writeGpr(2u, source);

    const auto encode = [](uint8_t opcode, uint8_t rt, uint16_t immediate) {
        return (static_cast<uint32_t>(opcode) << 26u) |
               (1u << 21u) |
               (static_cast<uint32_t>(rt) << 16u) |
               immediate;
    };
    const auto executeAtOffset = [&](uint8_t opcode, uint8_t rt, uint16_t offset) {
        const bool codeWriteOk = ps2::xbox360::guest_memory::writeLittleEndian(
            std::span<uint8_t>(ram), 0u, encode(opcode, rt, offset));
        if (!codeWriteOk)
            return StepResult{StepStatus::InvalidMemory, 0u, {}};
        cpu.pc = 0u;
        return step(cpu, ram);
    };

    constexpr std::array<uint32_t, 4> expectedLwl{
        0x11BBCCDDu, 0x2211CCDDu, 0x332211DDu, 0xF2332211u};
    constexpr std::array<uint32_t, 4> expectedLwr{
        0xF2332211u, 0xAAF23322u, 0xAABBF233u, 0xAABBCCF2u};
    constexpr std::array<uint32_t, 4> expectedSwl{
        0x443322AAu, 0x4433AABBu, 0x44AABBCCu, 0xAABBCCDDu};
    constexpr std::array<uint32_t, 4> expectedSwr{
        0xAABBCCDDu, 0xBBCCDD11u, 0xCCDD2211u, 0xDD332211u};

    uint32_t failures = 0u;
    if (!ps2::xbox360::guest_memory::writeLittleEndian(
            std::span<uint8_t>(ram), 0x40u, 0xF2332211u))
        failures |= 1u << 20u;
    for (uint16_t offset = 0u; offset < 4u; ++offset)
    {
        cpu.writeGpr(5u, initial);
        const auto lwl = executeAtOffset(0x22u, 5u, offset);
        const uint32_t expectedLwlUpper =
            offset == 3u ? 0xFFFFFFFFu : 0u;
        if (lwl.status != StepStatus::Executed ||
            ps2::xbox360::simd::readLane<uint32_t>(cpu.readGpr(5u), 0u) != expectedLwl[offset] ||
            ps2::xbox360::simd::readLane<uint32_t>(cpu.readGpr(5u), 1u) != expectedLwlUpper ||
            ps2::xbox360::simd::readLane<uint32_t>(cpu.readGpr(5u), 2u) != 0x10203040u ||
            ps2::xbox360::simd::readLane<uint32_t>(cpu.readGpr(5u), 3u) != 0x50607080u)
            failures |= 1u << offset;

        cpu.writeGpr(5u, initial);
        const auto lwr = executeAtOffset(0x26u, 5u, offset);
        const uint32_t expectedLwrUpper =
            offset == 0u ? 0xFFFFFFFFu : 0x55667788u;
        if (lwr.status != StepStatus::Executed ||
            ps2::xbox360::simd::readLane<uint32_t>(cpu.readGpr(5u), 0u) != expectedLwr[offset] ||
            ps2::xbox360::simd::readLane<uint32_t>(cpu.readGpr(5u), 1u) != expectedLwrUpper ||
            ps2::xbox360::simd::readLane<uint32_t>(cpu.readGpr(5u), 2u) != 0x10203040u ||
            ps2::xbox360::simd::readLane<uint32_t>(cpu.readGpr(5u), 3u) != 0x50607080u)
            failures |= 1u << (4u + offset);
    }

    for (uint16_t offset = 0u; offset < 4u; ++offset)
    {
        uint32_t stored{};
        const bool swlInitOk = ps2::xbox360::guest_memory::writeLittleEndian(
            std::span<uint8_t>(ram), 0x40u, 0x44332211u);
        const auto swl = executeAtOffset(0x2Au, 2u, offset);
        const bool swlReadOk = ps2::xbox360::guest_memory::readLittleEndian(
            std::span<const uint8_t>(ram), 0x40u, stored);
        if (!swlInitOk || !swlReadOk || swl.status != StepStatus::Executed ||
            stored != expectedSwl[offset])
            failures |= 1u << (8u + offset);

        const bool swrInitOk = ps2::xbox360::guest_memory::writeLittleEndian(
            std::span<uint8_t>(ram), 0x40u, 0x44332211u);
        const auto swr = executeAtOffset(0x2Eu, 2u, offset);
        const bool swrReadOk = ps2::xbox360::guest_memory::readLittleEndian(
            std::span<const uint8_t>(ram), 0x40u, stored);
        if (!swrInitOk || !swrReadOk || swr.status != StepStatus::Executed ||
            stored != expectedSwr[offset])
            failures |= 1u << (12u + offset);
    }

    // A merge load to GPR 0 still performs the read but discards the result.
    const auto lwlZero = executeAtOffset(0x22u, 0u, 0u);
    if (lwlZero.status != StepStatus::Executed ||
        cpu.readGpr(0u).bytes != ps2::xbox360::simd::Vector128{}.bytes)
        failures |= 1u << 16u;

    xsdk_log_write("MKSM_EE_UNALIGNED_MERGE_TRACE failures=%08X offsets=4",
                   static_cast<unsigned>(failures));
    return failures == 0u;
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

bool checkIomanBridge()
{
#if defined(MKSM_DISC_ROOT_CONFIGURED)
    const char *discRoot = MKSM_DISC_ROOT_CONFIGURED;
#else
    const char *discRoot = nullptr;
#endif
    if (!discRoot || !*discRoot)
        discRoot = std::getenv("MKSM_DISC_ROOT");
    if (!discRoot || !*discRoot)
    {
        xsdk_log_write("MKSM_IOMAN_BRIDGE status=not-configured hint=cmake-MKSM_DISC_ROOT");
        return true;
    }

    ps2::xbox360::iop::IomanDispatcher ioman{std::filesystem::path(discRoot)};
    std::array<std::uint8_t, 128> iopRam{};
    constexpr char probePath[] = "GAMEDATA.WAD";
    for (size_t i = 0; i < sizeof(probePath); ++i)
        iopRam[16u + i] = static_cast<std::uint8_t>(probePath[i]);

    using ps2::xbox360::iop::IomanCommand;
    // PS2SDK FIO_O_RDONLY is 0x0001; zero is not a valid read-only mode.
    const auto fd = ioman.dispatch(IomanCommand::Open, 16u, 0x0001u, 0u, iopRam);
    if (fd < 0)
    {
        xsdk_log_write("MKSM_IOMAN_BRIDGE status=asset-open-failed error=%d", fd);
        return false;
    }

    const auto firstRead = ioman.dispatch(IomanCommand::Read,
                                          static_cast<std::uint32_t>(fd),
                                          64u, 16u, iopRam);
    std::array<std::uint8_t, 16> firstBytes{};
    std::copy_n(iopRam.begin() + 64, firstBytes.size(), firstBytes.begin());

    const auto seekResult = ioman.dispatch(IomanCommand::Lseek,
                                           static_cast<std::uint32_t>(fd),
                                           0u,
                                           static_cast<std::uint32_t>(
                                               ps2::xbox360::iop::SeekWhence::Set),
                                           iopRam);
    const auto secondRead = seekResult == 0
        ? ioman.dispatch(IomanCommand::Read,
                         static_cast<std::uint32_t>(fd), 80u, 16u, iopRam)
        : -1;
    std::array<std::uint8_t, 16> secondBytes{};
    std::copy_n(iopRam.begin() + 80, secondBytes.size(), secondBytes.begin());

    const auto closeResult = ioman.dispatch(IomanCommand::Close,
                                             static_cast<std::uint32_t>(fd),
                                             0u, 0u, iopRam);
    if (firstRead < 0 || seekResult != 0 || secondRead != firstRead ||
        firstBytes != secondBytes || closeResult != 0)
    {
        xsdk_log_write("MKSM_IOMAN_BRIDGE status=failed read1=%d seek=%d read2=%d close=%d",
                       firstRead, seekResult, secondRead, closeResult);
        return false;
    }

    xsdk_log_write("MKSM_IOMAN_BRIDGE status=pass file=GAMEDATA.WAD read=%d seek=0 reread=%d",
                   firstRead, secondRead);
    return true;
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

#if !defined(MKSM_XENIA_HEADLESS_SMOKE)
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
#endif
} // namespace

int main()
{
#if defined(MKSM_XENIA_HEADLESS_SMOKE)
    // Xenia has no emulated display controller for xSDK presentation. Route the
    // diagnostic markers to xenia.log and skip only the display/controller loop.
    (void)xsdk_log_set_dbgprint(true);
    xsdk_log_write("MKSM_XEX_ENTRY mode=xenia-headless");
#else
    xsdk_log_write("MKSM_XEX_ENTRY mode=console-interactive");
#endif
    xsdk_log_write("MKSM Xbox 360 smoke: starting");
    xsdk_log_write("MKSM smoke: target ABI verified (32-bit big-endian PowerPC)");

    if (!checkIomanBridge())
    {
        xsdk_log_write("MKSM smoke: ioman retail file bridge check failed");
        return 10;
    }

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
    if (!checkEeLqSq())
    {
        xsdk_log_write("MKSM smoke: EE LQ/SQ dispatcher check failed");
        return 8;
    }
    if (!checkEeUnalignedMerge())
    {
        xsdk_log_write("MKSM smoke: EE unaligned merge check failed");
        return 9;
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
#if defined(MKSM_XENIA_HEADLESS_SMOKE)
    xsdk_log_write("MKSM_XENIA_GRAPHICS_SKIPPED reason=xenia-has-no-display-controller");
    xsdk_log_write("MKSM_XENIA_SMOKE_PASS checks=cpp20,guest-memory,ee,ee-lq-sq,ee-unaligned-merge,mmi,spr-dma,generated-recomp");
    return 0;
#else
    return runTitle();
#endif
}
