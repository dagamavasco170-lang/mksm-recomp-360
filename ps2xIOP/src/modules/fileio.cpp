#include "../iop_service.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace ps2x::iop::detail
{
    namespace
    {
        constexpr uint32_t kFileIoSid = 0x80000001u;
        constexpr int32_t kFioOpen = 0;
        constexpr int32_t kFioClose = 1;
        constexpr int32_t kFioRead = 2;
        constexpr int32_t kFioWrite = 3;
        constexpr int32_t kFioLseek = 4;
        constexpr int32_t kFioOpenReadOnly = 0x0001;
        constexpr int32_t kFioSeekSet = 0;
        constexpr int32_t kFioSeekCurrent = 1;
        constexpr int32_t kFioSeekEnd = 2;
        constexpr std::size_t kPathBytes = 256u;
        constexpr std::size_t kMaxReadBytes = 16u * 1024u * 1024u;

        struct alignas(16) FioOpenRequest
        {
            int32_t mode = 0;
            char name[kPathBytes]{};
        };

        struct alignas(16) FioCloseRequest
        {
            int32_t descriptor = -1;
        };

        struct alignas(16) FioReadRequest
        {
            int32_t descriptor = -1;
            uint32_t destination = 0u;
            int32_t size = 0;
            uint32_t readData = 0u;
        };

        struct alignas(16) FioReadData
        {
            uint32_t size1 = 0u;
            uint32_t size2 = 0u;
            uint32_t destination1 = 0u;
            uint32_t destination2 = 0u;
            uint8_t buffer1[16]{};
            uint8_t buffer2[16]{};
        };

        struct alignas(16) FioLseekRequest
        {
            int32_t descriptor = -1;
            int32_t offset = 0;
            int32_t whence = 0;
            uint32_t reserved = 0u;
        };

        static_assert(sizeof(FioOpenRequest) == 272u);
        static_assert(sizeof(FioCloseRequest) == 16u);
        static_assert(sizeof(FioReadRequest) == 16u);
        static_assert(sizeof(FioReadData) == 48u);
        static_assert(sizeof(FioLseekRequest) == 16u);

        class FileIoService final : public IopService
        {
        public:
            explicit FileIoService(IopHost &host) : m_host(host) {}

            ~FileIoService() override { reset(); }

            [[nodiscard]] std::string_view name() const override { return "FILEIO"; }
            [[nodiscard]] std::span<const uint32_t> sids() const override { return kSids; }

            void reset() override
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                for (auto &entry : m_files)
                {
                    if (entry.open)
                        m_host.closeHostFile(entry.hostHandle);
                    entry = {};
                }
            }

            [[nodiscard]] RpcResult handleRpc(const RpcRequest &request) override
            {
                if (request.sid != kFileIoSid)
                    return {};

                RpcResult response{};
                response.handled = true;
                response.resultAddress = request.receive.address;

                int32_t result = -1;
                switch (static_cast<int32_t>(request.function))
                {
                case kFioOpen:
                    result = open(request);
                    break;
                case kFioClose:
                    result = close(request);
                    break;
                case kFioRead:
                    result = read(request);
                    break;
                case kFioLseek:
                    result = seek(request);
                    break;
                case kFioWrite:
                    result = -13; // -EACCES: the retail-disc service is read-only.
                    break;
                default:
                    result = -38; // -ENOSYS
                    break;
                }

                if (request.receive.address != 0u && request.receive.size >= sizeof(result))
                    (void)m_host.writeGuest(request.receive.address, &result, sizeof(result));
                return response;
            }

        private:
            struct FileEntry
            {
                uint64_t hostHandle = 0u;
                uint64_t size = 0u;
                uint64_t position = 0u;
                bool open = false;
            };

            static constexpr std::array<uint32_t, 1> kSids{kFileIoSid};
            static constexpr int32_t kFirstDescriptor = 3;

            int32_t open(const RpcRequest &request)
            {
                if (request.send.address == 0u || request.send.size < sizeof(FioOpenRequest))
                    return -14; // -EFAULT

                FioOpenRequest packet{};
                if (!m_host.readGuest(request.send.address, &packet, sizeof(packet)))
                    return -14;
                if (packet.mode != kFioOpenReadOnly)
                    return -13;
                const void *terminator = std::memchr(packet.name, 0, sizeof(packet.name));
                if (!terminator)
                    return -22;

                const std::string_view guestPath(packet.name,
                    static_cast<const char *>(terminator) - packet.name);
                if (guestPath.empty())
                    return -22;
                const std::string hostPath = m_host.translateGuestPath(guestPath);
                if (hostPath.empty())
                    return -22;

                const uint64_t handle = m_host.openHostFile(hostPath);
                if (handle == 0u)
                    return -2; // -ENOENT

                uint64_t size = 0u;
                if (!m_host.hostFileSize(handle, size) ||
                    size > static_cast<uint64_t>(std::numeric_limits<int32_t>::max()))
                {
                    m_host.closeHostFile(handle);
                    return -75; // -EOVERFLOW
                }

                std::lock_guard<std::mutex> lock(m_mutex);
                for (std::size_t i = 0; i < m_files.size(); ++i)
                {
                    if (!m_files[i].open)
                    {
                        m_files[i] = {handle, size, 0u, true};
                        return kFirstDescriptor + static_cast<int32_t>(i);
                    }
                }
                m_host.closeHostFile(handle);
                return -24; // -EMFILE
            }

            int32_t close(const RpcRequest &request)
            {
                FioCloseRequest packet{};
                if (request.send.address == 0u ||
                    request.send.size < sizeof(int32_t) ||
                    !m_host.readGuest(request.send.address, &packet.descriptor, sizeof(packet.descriptor)))
                    return -14;

                std::lock_guard<std::mutex> lock(m_mutex);
                FileEntry *entry = find(packet.descriptor);
                if (!entry)
                    return -9; // -EBADF
                m_host.closeHostFile(entry->hostHandle);
                *entry = {};
                return 0;
            }

            int32_t read(const RpcRequest &request)
            {
                FioReadRequest packet{};
                if (request.send.address == 0u || request.send.size < sizeof(packet) ||
                    !m_host.readGuest(request.send.address, &packet, sizeof(packet)))
                    return -14;
                if (packet.size < 0 || static_cast<uint32_t>(packet.size) > kMaxReadBytes)
                    return -22;
                if (packet.size != 0 && packet.destination == 0u)
                    return -14;

                std::lock_guard<std::mutex> lock(m_mutex);
                FileEntry *entry = find(packet.descriptor);
                if (!entry)
                    return -9;
                const uint64_t remaining = entry->position < entry->size
                                               ? entry->size - entry->position
                                               : 0u;
                const std::size_t requested = static_cast<std::size_t>(packet.size);
                const std::size_t count = static_cast<std::size_t>(
                    std::min<uint64_t>(remaining, requested));
                std::vector<uint8_t> bytes(count);
                std::size_t bytesRead = 0u;
                if (count != 0u &&
                    !m_host.readHostFile(entry->hostHandle, entry->position,
                                         bytes.data(), count, bytesRead))
                    return -5;
                if (bytesRead != 0u &&
                    !m_host.writeGuest(packet.destination, bytes.data(), bytesRead))
                    return -14;

                // The standard EE fioRead completion callback copies only these
                // bounce-buffer fragments. We have already copied the whole range
                // to guest RDRAM, so report an empty bounce packet.
                if (packet.readData != 0u)
                {
                    const FioReadData empty{};
                    if (!m_host.writeGuest(packet.readData, &empty, sizeof(empty)))
                        return -14;
                }

                entry->position += bytesRead;
                return static_cast<int32_t>(bytesRead);
            }

            int32_t seek(const RpcRequest &request)
            {
                FioLseekRequest packet{};
                if (request.send.address == 0u || request.send.size < 12u ||
                    !m_host.readGuest(request.send.address, &packet, 12u))
                    return -14;

                std::lock_guard<std::mutex> lock(m_mutex);
                FileEntry *entry = find(packet.descriptor);
                if (!entry)
                    return -9;

                int64_t base = 0;
                switch (packet.whence)
                {
                case kFioSeekSet: base = 0; break;
                case kFioSeekCurrent: base = static_cast<int64_t>(entry->position); break;
                case kFioSeekEnd: base = static_cast<int64_t>(entry->size); break;
                default: return -22;
                }
                const int64_t next = base + static_cast<int64_t>(packet.offset);
                if (next < 0 || next > std::numeric_limits<int32_t>::max())
                    return -22;
                entry->position = static_cast<uint64_t>(next);
                return static_cast<int32_t>(next);
            }

            FileEntry *find(int32_t descriptor)
            {
                const int32_t index = descriptor - kFirstDescriptor;
                if (index < 0 || static_cast<std::size_t>(index) >= m_files.size())
                    return nullptr;
                FileEntry &entry = m_files[static_cast<std::size_t>(index)];
                return entry.open ? &entry : nullptr;
            }

            IopHost &m_host;
            std::mutex m_mutex;
            std::array<FileEntry, 32> m_files{};
        };
    }

    std::unique_ptr<IopService> createFileIoService(IopHost &host)
    {
        return std::make_unique<FileIoService>(host);
    }
}
