#include "ps2_generated_runtime_abi.h"

#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <vector>

int main()
{
    std::vector<std::uint8_t> rdram(
        ps2::xbox360::generated_abi::kGuestRdramSize, 0u);
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

    // Cross-window coherence: the aliases must refer to the same byte storage.
    writeRdram<std::uint32_t>(rdram.data(), 0x00100100u, 0x1234ABCDu);
    expect(readRdram<std::uint32_t>(rdram.data(), 0x20100100u) == 0x1234ABCDu,
           "cached write visible through uncached alias");
    expect(readRdram<std::uint32_t>(rdram.data(), 0x30100100u) == 0x1234ABCDu,
           "cached write visible through accelerated alias");
    expect(readRdram<std::uint32_t>(rdram.data(), 0x80100100u) == 0x1234ABCDu,
           "cached write visible through KSEG0 alias");
    expect(readRdram<std::uint32_t>(rdram.data(), 0xA0100100u) == 0x1234ABCDu,
           "cached write visible through KSEG1 alias");

    writeRdram<std::uint64_t>(rdram.data(), 0x20100200u, 0x0123456789ABCDEFull);
    expect(readRdram<std::uint64_t>(rdram.data(), 0x00100200u) == 0x0123456789ABCDEFull,
           "uncached write visible in default window");
    expect(readRdram<std::uint64_t>(rdram.data(), 0x30100200u) == 0x0123456789ABCDEFull,
           "uncached write visible in accelerated alias");

    writeRdram<std::uint16_t>(rdram.data(), 0x30100300u, 0xBEEFu);
    expect(readRdram<std::uint16_t>(rdram.data(), 0x20100300u) == 0xBEEFu,
           "accelerated write visible in uncached alias");
    expect(readRdram<std::uint16_t>(rdram.data(), 0x80100300u) == 0xBEEFu,
           "accelerated write visible through KSEG0");

    bool ioRejected = false;
    bool scratchRejected = false;
    bool badWindowRejected = false;
    try { (void)readRdram<std::uint32_t>(rdram.data(), 0x10000000u); }
    catch (const std::out_of_range &) { ioRejected = true; }
    catch (...) {}
    expect(ioRejected, "MMIO read is rejected rather than aliased to RDRAM");

    try { writeRdram<std::uint32_t>(rdram.data(), 0x70000000u, 0u); }
    catch (const std::out_of_range &) { scratchRejected = true; }
    catch (...) {}
    expect(scratchRejected, "scratchpad write is rejected");

    try { (void)readRdram<std::uint8_t>(rdram.data(), 0x02000000u); }
    catch (const std::out_of_range &) { badWindowRejected = true; }
    catch (...) {}
    expect(badWindowRejected, "undocumented address gap is rejected");

    std::printf("MKSM_GENERATED_ABI_MEMORY_TEST cases=%u failures=%u\n",
                static_cast<unsigned>(cases), static_cast<unsigned>(failures));
    return failures == 0u ? 0 : 1;
}
