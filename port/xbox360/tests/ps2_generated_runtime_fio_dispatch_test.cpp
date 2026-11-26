#include "ps2_generated_runtime_abi.h"

#include <array>
#include <cassert>
#include <cerrno>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

int main()
{
    namespace fs = std::filesystem;
    using ps2::xbox360::iop::IomanDispatcher;
    const fs::path root = fs::temp_directory_path() / "mksm_generated_fio_dispatch_test";
    fs::remove_all(root);
    fs::create_directories(root);
    {
        std::ofstream f(root / "generated_fio.bin", std::ios::binary);
        constexpr std::array<std::uint8_t, 5> data{0x9A, 0xBC, 0xDE, 0xF0, 0x12};
        f.write(reinterpret_cast<const char*>(data.data()), data.size());
    }
    IomanDispatcher dispatcher(root);
    PS2Runtime runtime;
    runtime.attachIoman(&dispatcher);
    std::vector<std::uint8_t> ram(ps2::xbox360::rdram_address::kRdramSize);
    R5900Context ctx{};
    constexpr std::uint32_t pathAddress = 0x1000, readBuffer = 0x2000;
    constexpr char path[] = "generated_fio.bin";
    for (std::size_t i=0; i<sizeof(path); ++i)
        ram[pathAddress+i] = static_cast<std::uint8_t>(path[i]);

    ctx.r[4].low64 = pathAddress;
    ctx.r[5].low64 = 1u; // PS2 FIO_O_RDONLY
    ps2XboxFioOpen(ram.data(), &ctx, &runtime);
    const auto fd = static_cast<std::int32_t>(ctx.r[2].low64);
    assert(fd >= IomanDispatcher::AssetServiceFirstDescriptor ||
           fd >= ps2::xbox360::iop::RetailFileIo::FirstDescriptor);
    ctx.r[4].low64 = static_cast<std::uint32_t>(fd);
    ctx.r[5].low64 = readBuffer;
    ctx.r[6].low64 = 3;
    ps2XboxFioRead(ram.data(), &ctx, &runtime);
    assert(static_cast<std::int32_t>(ctx.r[2].low64) == 3);
    assert((std::array<std::uint8_t,3>{ram[readBuffer],ram[readBuffer+1],ram[readBuffer+2]} ==
            std::array<std::uint8_t,3>{0x9A,0xBC,0xDE}));

    ctx.r[4].low64 = static_cast<std::uint32_t>(fd);
    ctx.r[5].low64 = 1;
    ctx.r[6].low64 = 0; // SEEK_SET
    ps2XboxFioLseek(ram.data(), &ctx, &runtime);
    assert(static_cast<std::int32_t>(ctx.r[2].low64) == 1);
    ctx.r[4].low64 = static_cast<std::uint32_t>(fd);
    ctx.r[5].low64 = readBuffer + 8;
    ctx.r[6].low64 = 2;
    ps2XboxFioRead(ram.data(), &ctx, &runtime);
    assert(static_cast<std::int32_t>(ctx.r[2].low64) == 2);
    assert((std::array<std::uint8_t,2>{ram[readBuffer+8],ram[readBuffer+9]} ==
            std::array<std::uint8_t,2>{0xBC,0xDE}));

    ctx.r[4].low64 = pathAddress;
    ctx.r[5].low64 = 3; // read/write must not be silently downgraded
    ps2XboxFioOpen(ram.data(), &ctx, &runtime);
    assert(static_cast<std::int32_t>(ctx.r[2].low64) == -EACCES);
    ps2XboxFioWrite(ram.data(), &ctx, &runtime);
    assert(static_cast<std::int32_t>(ctx.r[2].low64) == -EACCES);

    ctx.r[4].low64 = static_cast<std::uint32_t>(fd);
    ps2XboxFioClose(ram.data(), &ctx, &runtime);
    assert(static_cast<std::int32_t>(ctx.r[2].low64) == 0);
    assert(dispatcher.openFileCount() == 0);
    fs::remove_all(root);
    return 0;
}
