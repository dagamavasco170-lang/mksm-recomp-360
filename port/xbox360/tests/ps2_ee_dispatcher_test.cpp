#include "ps2_ee_dispatcher.h"

#include <array>
#include <cassert>
#include <cstdint>

using namespace ps2::xbox360::ee;

int main()
{
    std::array<std::uint8_t, 256> ram{};
    CpuState cpu{};
    cpu.pc = 0x20;
    // ADDU r3, r1, r2
    const std::uint32_t addu = (1u << 21) | (2u << 16) | (3u << 11) | 0x21u;
    assert(guest_memory::writeLittleEndian(std::span<std::uint8_t>(ram), cpu.pc, addu));
    simd::Vector128 one{}, two{};
    simd::writeLane<std::uint32_t>(one, 0, 7u);
    simd::writeLane<std::uint32_t>(two, 0, 9u);
    cpu.writeGpr(1, one);
    cpu.writeGpr(2, two);
    auto result = step(cpu, ram);
    assert(result.status == StepStatus::Executed);
    assert(simd::readLane<std::uint32_t>(cpu.readGpr(3), 0) == 16u);
    assert(cpu.pc == 0x24u && cpu.retiredInstructions == 1u);

    // An unsupported COP instruction is reported without advancing the CPU.
    const std::uint32_t cop1 = 0x44000000u;
    assert(guest_memory::writeLittleEndian(std::span<std::uint8_t>(ram), cpu.pc, cop1));
    result = step(cpu, ram);
    assert(result.status == StepStatus::Unsupported);
    assert(cpu.pc == 0x24u && cpu.retiredInstructions == 1u);

    // Fetch from an invalid RAM span is rejected.
    std::span<std::uint8_t> empty{};
    result = step(cpu, empty);
    assert(result.status == StepStatus::InvalidMemory);
    assert(cpu.pc == 0x24u && cpu.retiredInstructions == 1u);
}
