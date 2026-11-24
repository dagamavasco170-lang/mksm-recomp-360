#include "ps2_generated_runtime_abi.h"

#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <vector>

int main()
{
    std::vector<std::uint8_t> rdramStorage(
        ps2::xbox360::generated_abi::kGuestRdramSize, 0u);
    std::uint8_t *rdram = rdramStorage.data();
    PS2Runtime runtimeStorage{};
    PS2Runtime *runtime = &runtimeStorage;
    PS2Runtime secondRuntimeStorage{};
    PS2Runtime *secondRuntime = &secondRuntimeStorage;
    std::uint32_t cases = 0u;
    std::uint32_t failures = 0u;

    auto expect = [&](bool condition, const char *name) {
        ++cases;
        if (!condition) {
            ++failures;
            std::printf("FAIL %s\n", name);
        }
    };

    using ps2::xbox360::generated_abi::readRdram;
    using ps2::xbox360::generated_abi::writeRdram;
    using ps2::xbox360::generated_abi::readGuestMemory;
    using ps2::xbox360::generated_abi::writeGuestMemory;

    // Cross-window coherence: the aliases must refer to the same byte storage.
    writeRdram<std::uint32_t>(rdram, 0x00100100u, 0x1234ABCDu);
    expect(readRdram<std::uint32_t>(rdram, 0x20100100u) == 0x1234ABCDu,
           "cached write visible through uncached alias");
    expect(readRdram<std::uint32_t>(rdram, 0x30100100u) == 0x1234ABCDu,
           "cached write visible through accelerated alias");
    expect(readRdram<std::uint32_t>(rdram, 0x80100100u) == 0x1234ABCDu,
           "cached write visible through KSEG0 alias");
    expect(readRdram<std::uint32_t>(rdram, 0xA0100100u) == 0x1234ABCDu,
           "cached write visible through KSEG1 alias");

    writeRdram<std::uint64_t>(rdram, 0x20100200u, 0x0123456789ABCDEFull);
    expect(readRdram<std::uint64_t>(rdram, 0x00100200u) == 0x0123456789ABCDEFull,
           "uncached write visible in default window");
    expect(readRdram<std::uint64_t>(rdram, 0x30100200u) == 0x0123456789ABCDEFull,
           "uncached write visible in accelerated alias");

    writeRdram<std::uint16_t>(rdram, 0x30100300u, 0xBEEFu);
    expect(readRdram<std::uint16_t>(rdram, 0x20100300u) == 0xBEEFu,
           "accelerated write visible in uncached alias");
    expect(readRdram<std::uint16_t>(rdram, 0x80100300u) == 0xBEEFu,
           "accelerated write visible through KSEG0");

    bool ioRejected = false;
    bool scratchRejected = false;
    bool badWindowRejected = false;
    try { (void)readRdram<std::uint32_t>(rdram, 0x10000000u); }
    catch (const std::out_of_range &) { ioRejected = true; }
    catch (...) {}
    expect(ioRejected, "MMIO read is rejected rather than aliased to RDRAM");

    try { writeRdram<std::uint32_t>(rdram, 0x70000000u, 0u); }
    catch (const std::out_of_range &) { scratchRejected = true; }
    catch (...) {}
    expect(scratchRejected, "RDRAM-only helper still rejects scratchpad");

    try { (void)readRdram<std::uint8_t>(rdram, 0x02000000u); }
    catch (const std::out_of_range &) { badWindowRejected = true; }
    catch (...) {}
    expect(badWindowRejected, "undocumented address gap is rejected");

    // Scratchpad is independently backed by the EE runtime and uses bounded,
    // little-endian scalar accesses rather than the wrapping RDRAM helper.
    expect(readGuestMemory<std::uint32_t>(rdram, runtime, 0x70000000u) == 0u,
           "scratchpad starts zero-initialized");
    writeGuestMemory<std::uint8_t>(rdram, runtime, 0x70000001u, 0x5Au);
    writeGuestMemory<std::uint16_t>(rdram, runtime, 0x70000002u, 0xBEEFu);
    writeGuestMemory<std::uint32_t>(rdram, runtime, 0x70000004u, 0x12345678u);
    writeGuestMemory<std::uint64_t>(rdram, runtime, 0x70000008u,
                                    0x0123456789ABCDEFull);
    expect(readGuestMemory<std::uint8_t>(rdram, runtime, 0x70000001u) == 0x5Au,
           "scratchpad byte read/write");
    expect(readGuestMemory<std::uint16_t>(rdram, runtime, 0x70000002u) == 0xBEEFu,
           "scratchpad halfword read/write");
    expect(readGuestMemory<std::uint32_t>(rdram, runtime, 0x70000004u) == 0x12345678u,
           "scratchpad word read/write");
    expect(readGuestMemory<std::uint64_t>(rdram, runtime, 0x70000008u) ==
               0x0123456789ABCDEFull,
           "scratchpad doubleword read/write");
    expect(runtime->eeScratchpad()[1] == 0x5Au &&
               runtime->eeScratchpad()[2] == 0xEFu &&
               runtime->eeScratchpad()[3] == 0xBEu,
           "scratchpad bytes are stored in guest little-endian order");
    expect(readGuestMemory<std::uint32_t>(rdram, runtime, 0x00000004u) == 0u,
           "scratchpad does not alias main RDRAM");
    expect(readGuestMemory<std::uint32_t>(rdram, secondRuntime, 0x70000004u) == 0u,
           "scratchpad storage is isolated per EE runtime");
    writeGuestMemory<std::uint32_t>(rdram, secondRuntime, 0x70000004u, 0xCAFEBABEu);
    expect(readGuestMemory<std::uint32_t>(rdram, runtime, 0x70000004u) == 0x12345678u,
           "second runtime writes do not mutate the first scratchpad");

    writeGuestMemory<std::uint16_t>(rdram, runtime, 0x70003FFEu, 0xCAFEu);
    expect(readGuestMemory<std::uint16_t>(rdram, runtime, 0x70003FFEu) == 0xCAFEu,
           "last aligned halfword in scratchpad is accessible");
    writeGuestMemory<std::uint8_t>(rdram, runtime, 0x70003FFFu, 0xA5u);
    expect(readGuestMemory<std::uint8_t>(rdram, runtime, 0x70003FFFu) == 0xA5u,
           "last scratchpad byte is accessible");

    bool scratchReadOverrunRejected = false;
    bool scratchWriteOverrunRejected = false;
    bool scratchNullRuntimeRejected = false;
    try { (void)readGuestMemory<std::uint16_t>(rdram, runtime, 0x70003FFFu); }
    catch (const std::out_of_range &) { scratchReadOverrunRejected = true; }
    catch (...) {}
    expect(scratchReadOverrunRejected, "scratchpad read cannot wrap past its end");

    try { writeGuestMemory<std::uint32_t>(rdram, runtime, 0x70003FFEu, 0u); }
    catch (const std::out_of_range &) { scratchWriteOverrunRejected = true; }
    catch (...) {}
    expect(scratchWriteOverrunRejected, "scratchpad write cannot wrap past its end");

    try { (void)readGuestMemory<std::uint8_t>(rdram, nullptr, 0x70000000u); }
    catch (const std::invalid_argument &) { scratchNullRuntimeRejected = true; }
    catch (...) {}
    expect(scratchNullRuntimeRejected, "scratchpad access requires a live EE runtime");

    expect(readGuestMemory<std::uint32_t>(rdram, runtime, 0x00100100u) ==
               0x1234ABCDu,
           "guest-memory dispatcher preserves RDRAM mappings");

    // Exercise the exact memory macros emitted by PS2Recomp as well.
    WRITE32(0x70000020u, 0x89ABCDEFu);
    expect(READ32(0x70000020u) == 0x89ABCDEFu,
           "generated READ32/WRITE32 macros route to scratchpad");
    FAST_WRITE16(0x70000024u, 0x1357u);
    expect(FAST_READ16(0x70000024u) == 0x1357u,
           "generated FAST_READ/FAST_WRITE macros route to scratchpad");

    std::printf("MKSM_GENERATED_ABI_MEMORY_TEST cases=%u failures=%u\n",
                static_cast<unsigned>(cases), static_cast<unsigned>(failures));
    return failures == 0u ? 0 : 1;
}
