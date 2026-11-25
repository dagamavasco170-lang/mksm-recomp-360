#include "module_factories.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace
{
    using namespace ps2x::iop;

    class TestHost final : public IopHost
    {
    public:
        bool readGuest(uint32_t address, void *destination, size_t size) const override
        {
            if ((!destination && size != 0u) || address > memory.size() ||
                size > memory.size() - address)
                return false;
            if (size != 0u)
                std::memcpy(destination, memory.data() + address, size);
            return true;
        }

        bool writeGuest(uint32_t address, const void *source, size_t size) override
        {
            if ((!source && size != 0u) || address > memory.size() ||
                size > memory.size() - address)
                return false;
            if (size != 0u)
                std::memcpy(memory.data() + address, source, size);
            return true;
        }

        bool zeroGuest(uint32_t address, size_t size) override
        {
            if (address > memory.size() || size > memory.size() - address)
                return false;
            std::fill_n(memory.data() + address, size, uint8_t{0});
            return true;
        }

        bool normalizeGuestAddress(uint32_t address, uint32_t &normalized) const override
        {
            normalized = address;
            return address < memory.size();
        }

        uint32_t allocateIopHandle(IopHandleKind) override { return nextHandle++; }
        uint32_t allocateGuest(uint32_t, uint32_t) override { return 0u; }
        void freeGuest(uint32_t) override {}
        void audioCommand(uint32_t, uint32_t, GuestBuffer, GuestBuffer) override {}

        std::string hostPath(HostPathKind) const override { return "disc-root"; }
        std::string translateGuestPath(std::string_view path) const override
        {
            return "translated/" + std::string(path);
        }

        uint64_t openHostFile(std::string_view path) override
        {
            const auto it = files.find(std::string(path));
            if (it == files.end())
                return 0u;
            const uint64_t handle = nextFileHandle++;
            opened.emplace(handle, it->first);
            return handle;
        }

        bool hostFileSize(uint64_t handle, uint64_t &size) const override
        {
            const auto it = opened.find(handle);
            if (it == opened.end())
                return false;
            size = files.at(it->second).size();
            return true;
        }

        bool readHostFile(uint64_t handle, uint64_t offset, void *destination,
                          size_t size, size_t &bytesRead) override
        {
            bytesRead = 0u;
            const auto it = opened.find(handle);
            if (it == opened.end() || (!destination && size != 0u))
                return false;
            const auto &contents = files.at(it->second);
            if (offset > contents.size())
                return false;
            bytesRead = std::min<size_t>(size, contents.size() - static_cast<size_t>(offset));
            if (bytesRead != 0u)
                std::memcpy(destination, contents.data() + static_cast<size_t>(offset), bytesRead);
            return true;
        }

        void closeHostFile(uint64_t handle) override
        {
            opened.erase(handle);
            ++closedCount;
        }

        int32_t memoryCard(const MemoryCardRequest &) override { return 0; }
        bool hasGuestFunction(uint32_t) const override { return false; }
        bool invokeGuestFunction(uint64_t, uint32_t, uint32_t, uint32_t,
                                 uint32_t, uint32_t, uint32_t *) override
        {
            return false;
        }
        void log(LogLevel, std::string_view) override {}

        std::vector<uint8_t> memory = std::vector<uint8_t>(0x10000u);
        std::unordered_map<std::string, std::vector<uint8_t>> files;
        std::unordered_map<uint64_t, std::string> opened;
        uint32_t nextHandle = 0x100u;
        uint64_t nextFileHandle = 1u;
        uint32_t closedCount = 0u;
    };

    struct alignas(16) OpenPacket
    {
        int32_t mode = 1;
        char name[256]{};
    };
    struct alignas(16) ReadPacket
    {
        int32_t fd = -1;
        uint32_t destination = 0u;
        int32_t size = 0;
        uint32_t readData = 0u;
    };
    struct alignas(16) LseekPacket
    {
        int32_t fd = -1;
        int32_t offset = 0;
        int32_t whence = 0;
        uint32_t reserved = 0u;
    };

    template <typename T>
    void writePacket(TestHost &host, uint32_t address, const T &packet)
    {
        assert(host.writeGuest(address, &packet, sizeof(packet)));
    }

    int32_t readResult(const TestHost &host, uint32_t address)
    {
        int32_t result = -999;
        assert(host.readGuest(address, &result, sizeof(result)));
        return result;
    }
}

int main()
{
    constexpr uint32_t kFileIoSid = 0x80000001u;
    constexpr uint32_t kOpen = 0u;
    constexpr uint32_t kClose = 1u;
    constexpr uint32_t kRead = 2u;
    constexpr uint32_t kWrite = 3u;
    constexpr uint32_t kLseek = 4u;

    TestHost host;
    host.files["translated/cdrom0:/GAMEDATA.WAD;1"] =
        {'S', 'H', 'A', 'O', 'L', 'I', 'N', '!'};
    auto service = ps2x::iop::detail::createFileIoService(host);

    OpenPacket open{};
    open.mode = 1; // FIO_O_RDONLY
    std::strcpy(open.name, "cdrom0:/GAMEDATA.WAD;1");
    writePacket(host, 0x1000u, open);

    ps2x::iop::RpcRequest request{};
    request.sid = kFileIoSid;
    request.function = kOpen;
    request.send = {0x1000u, sizeof(open)};
    request.receive = {0x1100u, sizeof(int32_t)};
    auto response = service->handleRpc(request);
    assert(response.handled);
    const int32_t fd = readResult(host, 0x1100u);
    assert(fd >= 3);

    ReadPacket read{};
    read.fd = fd;
    read.destination = 0x2000u;
    read.size = 4;
    read.readData = 0x3000u;
    writePacket(host, 0x1200u, read);
    request.function = kRead;
    request.send = {0x1200u, sizeof(read)};
    request.receive = {0x1300u, sizeof(int32_t)};
    response = service->handleRpc(request);
    assert(response.handled);
    assert(readResult(host, 0x1300u) == 4);
    std::array<char, 4> first{};
    assert(host.readGuest(0x2000u, first.data(), first.size()));
    assert((first == std::array<char, 4>{'S', 'H', 'A', 'O'}));
    std::array<uint8_t, 48> bounce{};
    assert(host.readGuest(0x3000u, bounce.data(), bounce.size()));
    assert(std::all_of(bounce.begin(), bounce.end(), [](uint8_t b) { return b == 0u; }));

    LseekPacket seek{};
    seek.fd = fd;
    seek.offset = 4;
    seek.whence = 0; // FIO_SEEK_SET
    writePacket(host, 0x1400u, seek);
    request.function = kLseek;
    request.send = {0x1400u, 12u};
    request.receive = {0x1500u, sizeof(int32_t)};
    response = service->handleRpc(request);
    assert(response.handled);
    assert(readResult(host, 0x1500u) == 4);

    read.destination = 0x2100u;
    read.size = 8;
    writePacket(host, 0x1200u, read);
    request.function = kRead;
    request.send = {0x1200u, sizeof(read)};
    request.receive = {0x1300u, sizeof(int32_t)};
    response = service->handleRpc(request);
    assert(readResult(host, 0x1300u) == 4);
    std::array<char, 4> tail{};
    assert(host.readGuest(0x2100u, tail.data(), tail.size()));
    assert((tail == std::array<char, 4>{'L', 'I', 'N', '!'}));

    const int32_t closeFd = fd;
    assert(host.writeGuest(0x1600u, &closeFd, sizeof(closeFd)));
    request.function = kClose;
    request.send = {0x1600u, sizeof(closeFd)};
    request.receive = {0x1700u, sizeof(int32_t)};
    response = service->handleRpc(request);
    assert(response.handled);
    assert(readResult(host, 0x1700u) == 0);
    assert(host.opened.empty());
    assert(host.closedCount == 1u);

    request.function = kWrite;
    request.send = {0x1600u, sizeof(closeFd)};
    request.receive = {0x1700u, sizeof(int32_t)};
    response = service->handleRpc(request);
    assert(response.handled);
    assert(readResult(host, 0x1700u) == -13);

    return 0;
}
