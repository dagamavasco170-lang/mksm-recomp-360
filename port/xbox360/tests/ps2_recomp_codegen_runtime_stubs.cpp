#include "ps2_runtime.h"

#include <cstdint>
#include <stdexcept>

// The desktop reference executable links the generated function without
// constructing the full game runtime. These are link-only slow-path guards:
// every test address must resolve to ordinary RDRAM and therefore take the
// existing PS2 fast path. Reaching one is a test failure, not emulation.
std::uint8_t PS2Runtime::Load8(std::uint8_t *, R5900Context *, std::uint32_t)
{
    throw std::logic_error("codegen fixture unexpectedly used PS2Runtime::Load8");
}

std::uint16_t PS2Runtime::Load16(std::uint8_t *, R5900Context *, std::uint32_t)
{
    throw std::logic_error("codegen fixture unexpectedly used PS2Runtime::Load16");
}

std::uint32_t PS2Runtime::Load32(std::uint8_t *, R5900Context *, std::uint32_t)
{
    throw std::logic_error("codegen fixture unexpectedly used PS2Runtime::Load32");
}

std::uint64_t PS2Runtime::Load64(std::uint8_t *, R5900Context *, std::uint32_t)
{
    throw std::logic_error("codegen fixture unexpectedly used PS2Runtime::Load64");
}

void PS2Runtime::Store8(std::uint8_t *, R5900Context *, std::uint32_t, std::uint8_t)
{
    throw std::logic_error("codegen fixture unexpectedly used PS2Runtime::Store8");
}

void PS2Runtime::Store16(std::uint8_t *, R5900Context *, std::uint32_t, std::uint16_t)
{
    throw std::logic_error("codegen fixture unexpectedly used PS2Runtime::Store16");
}

void PS2Runtime::Store32(std::uint8_t *, R5900Context *, std::uint32_t, std::uint32_t)
{
    throw std::logic_error("codegen fixture unexpectedly used PS2Runtime::Store32");
}

void PS2Runtime::Store64(std::uint8_t *, R5900Context *, std::uint32_t, std::uint64_t)
{
    throw std::logic_error("codegen fixture unexpectedly used PS2Runtime::Store64");
}
