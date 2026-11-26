#include "Common.h"
#include "FileIO.h"

namespace ps2_syscalls
{
    static int allocatePs2Fd(FILE *file)
    {
        if (!file)
            return -1;

        std::lock_guard<std::mutex> lock(g_fd_mutex);
        int fd = g_nextFd++;
        g_fileDescriptors[fd] = file;
        return fd;
    }

    struct VagAccumEntry
    {
        std::vector<uint8_t> data;
        uint32_t firstBufAddr = 0;
    };
    static std::unordered_map<int, VagAccumEntry> g_vagAccum;
    static std::mutex g_vagAccumMutex;
    static constexpr size_t kVagAccumMaxBytes = 16 * 1024 * 1024;

    static const char *translateFioMode(int ps2Flags)
    {
        bool read = (ps2Flags & PS2_FIO_O_RDONLY) || (ps2Flags & PS2_FIO_O_RDWR);
        bool write = (ps2Flags & PS2_FIO_O_WRONLY) || (ps2Flags & PS2_FIO_O_RDWR);
        bool append = (ps2Flags & PS2_FIO_O_APPEND);
        bool create = (ps2Flags & PS2_FIO_O_CREAT);
        bool truncate = (ps2Flags & PS2_FIO_O_TRUNC);

        if (read && write)
        {
            if (append)
                return "a+b";
            if (create && truncate)
                return "w+b";
            if (create)
                return "r+b";
            return "r+b";
        }
        else if (write)
        {
            if (append)
                return "ab";
            if (create && truncate)
                return "wb";
            if (create)
                return "r+b";
            return "r+b";
        }
        else if (read)
        {
            return "rb";
        }
        return "rb";
    }

    void fioOpen(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        uint32_t pathAddr = getRegU32(ctx, 4); // $a0
        int flags = (int)getRegU32(ctx, 5);    // $a1 (PS2 FIO flags)

        const char *ps2Path = reinterpret_cast<const char *>(getConstMemPtr(rdram, pathAddr));
        if (!ps2Path)
        {
            std::cerr << "fioOpen error: Invalid path address" << std::endl;
            setReturnS32(ctx, -1);
            return;
        }

        std::string hostPath = translatePs2Path(ps2Path);
        if (hostPath.empty())
        {
            std::cerr << "fioOpen error: Failed to translate path '" << ps2Path << "'" << std::endl;
            setReturnS32(ctx, -1);
            return;
        }

        const char *mode = translateFioMode(flags);
        RUNTIME_LOG("fioOpen: '" << hostPath << "' flags=0x" << std::hex << flags << std::dec << " mode='" << mode << "'");

        FILE *fp = ::fopen(hostPath.c_str(), mode);
        // O_CREAT without O_TRUNC must preserve existing contents. The host's
        // "a+b" mode incorrectly forces every write to EOF, so create missing
        // files only after a non-destructive open reports ENOENT.
        if (!fp && (flags & PS2_FIO_O_CREAT) && !(flags & PS2_FIO_O_TRUNC) && errno == ENOENT)
        {
            const char *createMode = (flags & PS2_FIO_O_RDWR) ? "w+b" : "w+b";
            fp = ::fopen(hostPath.c_str(), createMode);
        }
        if (!fp)
        {
            std::cerr << "fioOpen error: fopen failed for '" << hostPath << "': " << strerror(errno) << std::endl;
            setReturnS32(ctx, -1); // e.g., -ENOENT, -EACCES
            return;
        }

        int ps2Fd = allocatePs2Fd(fp);
        if (ps2Fd < 0)
        {
            std::cerr << "fioOpen error: Failed to allocate PS2 file descriptor" << std::endl;
            ::fclose(fp);
            setReturnS32(ctx, -1); // e.g., -EMFILE
            return;
        }

        // returns the PS2 file descriptor
        setReturnS32(ctx, ps2Fd);
    }

    void fioClose(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        int ps2Fd = (int)getRegU32(ctx, 4);

        int ret = -1;
        {
            // Keep descriptor lookup, stream close, and table removal atomic with
            // respect to fioRead/fioWrite/fioLseek. A FILE* must never outlive
            // the lock that protects its descriptor entry.
            std::lock_guard<std::mutex> lock(g_fd_mutex);
            const auto it = g_fileDescriptors.find(ps2Fd);
            if (it == g_fileDescriptors.end() || !it->second)
            {
                std::cerr << "fioClose warning: Invalid PS2 file descriptor " << ps2Fd << std::endl;
                setReturnS32(ctx, -1);
                return;
            }

            ret = ::fclose(it->second);
            g_fileDescriptors.erase(it);
        }

        {
            std::lock_guard<std::mutex> lock(g_vagAccumMutex);
            auto it = g_vagAccum.find(ps2Fd);
            if (it != g_vagAccum.end())
            {
                VagAccumEntry &e = it->second;
                if (e.data.size() >= 48)
                {
                    const uint32_t magic = (static_cast<uint32_t>(e.data[0]) << 24) |
                                           (static_cast<uint32_t>(e.data[1]) << 16) |
                                           (static_cast<uint32_t>(e.data[2]) << 8) |
                                           static_cast<uint32_t>(e.data[3]);
                    const uint32_t magicLE = (static_cast<uint32_t>(e.data[3]) << 24) |
                                             (static_cast<uint32_t>(e.data[2]) << 16) |
                                             (static_cast<uint32_t>(e.data[1]) << 8) |
                                             static_cast<uint32_t>(e.data[0]);
                    if (magic == 0x56414770u || magicLE == 0x56414770u)
                    {
                        if (runtime)
                            runtime->audioBackend().onVagTransferFromBuffer(
                                e.data.data(), static_cast<uint32_t>(e.data.size()),
                                e.firstBufAddr ? e.firstBufAddr : 0u);
                    }
                }
                g_vagAccum.erase(it);
            }
        }

        setReturnS32(ctx, ret == 0 ? 0 : -1);
    }

    void fioRead(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        int ps2Fd = (int)getRegU32(ctx, 4);   // $a0
        uint32_t bufAddr = getRegU32(ctx, 5); // $a1
        size_t size = getRegU32(ctx, 6);      // $a2

        if (size == 0)
        {
            std::lock_guard<std::mutex> lock(g_fd_mutex);
            const auto it = g_fileDescriptors.find(ps2Fd);
            setReturnS32(ctx, it != g_fileDescriptors.end() && it->second ? 0 : -1);
            return; // A zero-byte read does not access guest memory.
        }

        uint32_t guestOffset = 0;
        bool scratch = false;
        const bool validGuestRange = rdram && resolveEeGuestRange(bufAddr, size, guestOffset, scratch);
        uint8_t *hostBuf = validGuestRange ? getMemPtr(rdram, bufAddr) : nullptr;
        if (!hostBuf)
        {
            std::cerr << "fioRead error: Invalid buffer address for fd " << ps2Fd << std::endl;
            setReturnS32(ctx, -1); // -EFAULT
            return;
        }

        size_t bytesRead = 0;
        bool readError = false;
        int readErrorCode = 0;
        {
            std::lock_guard<std::mutex> lock(g_fd_mutex);
            const auto it = g_fileDescriptors.find(ps2Fd);
            if (it == g_fileDescriptors.end() || !it->second)
            {
                std::cerr << "fioRead error: Invalid file descriptor " << ps2Fd << std::endl;
                setReturnS32(ctx, -1); // -EBADF
                return;
            }

            FILE *const fp = it->second;
            bytesRead = fread(hostBuf, 1, size, fp);
            readError = bytesRead < size && ferror(fp);
            if (readError)
            {
                readErrorCode = errno;
                clearerr(fp);
            }
        }
        if (bytesRead > 0)
        {
            ps2TraceGuestRangeWrite(rdram, bufAddr, static_cast<uint32_t>(bytesRead), "fioRead", ctx);
        }

        if (readError)
        {
            std::cerr << "fioRead error: fread failed for fd " << ps2Fd << ": "
                      << strerror(readErrorCode) << std::endl;
            setReturnS32(ctx, -1);
            return;
        }

        {
            // Keep VAG accumulation tied to a still-open descriptor. The lock
            // order is descriptor table -> VAG accumulator; fioClose removes
            // the descriptor before clearing its accumulator entry.
            std::lock_guard<std::mutex> descriptorLock(g_fd_mutex);
            if (g_fileDescriptors.find(ps2Fd) != g_fileDescriptors.end())
            {
                std::lock_guard<std::mutex> accumulationLock(g_vagAccumMutex);
                auto it = g_vagAccum.find(ps2Fd);
                if (it != g_vagAccum.end())
                {
                    VagAccumEntry &e = it->second;
                    if (e.data.size() + bytesRead <= kVagAccumMaxBytes)
                        e.data.insert(e.data.end(), hostBuf, hostBuf + bytesRead);
                }
                else if (bytesRead >= 4)
                {
                    const uint32_t magic = (static_cast<uint32_t>(hostBuf[0]) << 24) |
                                           (static_cast<uint32_t>(hostBuf[1]) << 16) |
                                           (static_cast<uint32_t>(hostBuf[2]) << 8) |
                                           static_cast<uint32_t>(hostBuf[3]);
                    const uint32_t magicLE = (static_cast<uint32_t>(hostBuf[3]) << 24) |
                                             (static_cast<uint32_t>(hostBuf[2]) << 16) |
                                             (static_cast<uint32_t>(hostBuf[1]) << 8) |
                                             static_cast<uint32_t>(hostBuf[0]);
                    if (magic == 0x56414770u || magicLE == 0x56414770u)
                    {
                        VagAccumEntry &e = g_vagAccum[ps2Fd];
                        e.firstBufAddr = bufAddr;
                        if (bytesRead <= kVagAccumMaxBytes)
                            e.data.assign(hostBuf, hostBuf + bytesRead);
                    }
                }
            }
        }

        setReturnS32(ctx, (int32_t)bytesRead);
    }

    void fioWrite(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        int ps2Fd = (int)getRegU32(ctx, 4);   // $a0
        uint32_t bufAddr = getRegU32(ctx, 5); // $a1
        size_t size = getRegU32(ctx, 6);      // $a2

        if (size == 0)
        {
            std::lock_guard<std::mutex> lock(g_fd_mutex);
            const auto it = g_fileDescriptors.find(ps2Fd);
            setReturnS32(ctx, it != g_fileDescriptors.end() && it->second ? 0 : -1);
            return; // A zero-byte write does not access guest memory.
        }

        uint32_t guestOffset = 0;
        bool scratch = false;
        const bool validGuestRange = rdram && resolveEeGuestRange(bufAddr, size, guestOffset, scratch);
        const uint8_t *hostBuf = validGuestRange ? getConstMemPtr(rdram, bufAddr) : nullptr;
        if (!hostBuf)
        {
            std::cerr << "fioWrite error: Invalid buffer range for fd " << ps2Fd << std::endl;
            setReturnS32(ctx, -1); // -EFAULT
            return;
        }

        size_t bytesWritten = 0;
        bool writeError = false;
        {
            std::lock_guard<std::mutex> lock(g_fd_mutex);
            const auto it = g_fileDescriptors.find(ps2Fd);
            if (it == g_fileDescriptors.end() || !it->second)
            {
                setReturnS32(ctx, -1); // -EBADF
                return;
            }

            FILE *const fp = it->second;
            bytesWritten = ::fwrite(hostBuf, 1, size, fp);
            writeError = bytesWritten < size && ferror(fp);
            if (writeError)
            {
                clearerr(fp);
            }
        }
        if (writeError)
        {
            setReturnS32(ctx, -1); // -EIO, -ENOSPC etc.
            return;
        }

        // returns number of bytes written
        setReturnS32(ctx, (int32_t)bytesWritten);
    }

    void fioLseek(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        int ps2Fd = (int)getRegU32(ctx, 4);  // $a0
        int32_t offset = getRegU32(ctx, 5);  // $a1 (PS2 seems to use 32-bit offset here commonly)
        int whence = (int)getRegU32(ctx, 6); // $a2 (PS2 FIO_SEEK constants)

        int hostWhence;
        switch (whence)
        {
        case PS2_FIO_SEEK_SET:
            hostWhence = SEEK_SET;
            break;
        case PS2_FIO_SEEK_CUR:
            hostWhence = SEEK_CUR;
            break;
        case PS2_FIO_SEEK_END:
            hostWhence = SEEK_END;
            break;
        default:
            std::cerr << "fioLseek error: Invalid whence value " << whence << " for fd " << ps2Fd << std::endl;
            setReturnS32(ctx, -1); // -EINVAL
            return;
        }

        {
            std::lock_guard<std::mutex> lock(g_fd_mutex);
            const auto it = g_fileDescriptors.find(ps2Fd);
            if (it == g_fileDescriptors.end() || !it->second)
            {
                std::cerr << "fioLseek error: Invalid file descriptor " << ps2Fd << std::endl;
                setReturnS32(ctx, -1); // -EBADF
                return;
            }

            FILE *const fp = it->second;
            if (::fseek(fp, static_cast<long>(offset), hostWhence) != 0)
            {
                std::cerr << "fioLseek error: fseek failed for fd " << ps2Fd << ": "
                          << strerror(errno) << std::endl;
                setReturnS32(ctx, -1); // Return error code
                return;
            }

            const long newPos = ::ftell(fp);
            if (newPos < 0)
            {
                std::cerr << "fioLseek error: ftell failed after fseek for fd " << ps2Fd << ": "
                          << strerror(errno) << std::endl;
                setReturnS32(ctx, -1);
            }
            else if (newPos > 0xFFFFFFFFL)
            {
                std::cerr << "fioLseek warning: New position exceeds 32-bit for fd " << ps2Fd << std::endl;
                setReturnS32(ctx, -1);
            }
            else
            {
                setReturnS32(ctx, static_cast<int32_t>(newPos));
            }
        }
    }

    void fioMkdir(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        uint32_t pathAddr = getRegU32(ctx, 4); // $a0
        // int mode = (int)getRegU32(ctx, 5);  // $a1 - ignored on host

        const char *ps2Path = reinterpret_cast<const char *>(getConstMemPtr(rdram, pathAddr));
        if (!ps2Path)
        {
            std::cerr << "fioMkdir error: Invalid path address" << std::endl;
            setReturnS32(ctx, -1); // -EFAULT
            return;
        }
        std::string hostPath = translatePs2Path(ps2Path);
        if (hostPath.empty())
        {
            std::cerr << "fioMkdir error: Failed to translate path '" << ps2Path << "'" << std::endl;
            setReturnS32(ctx, -1);
            return;
        }
        std::error_code ec;
        bool success = std::filesystem::create_directory(hostPath, ec);

        if (!success && ec)
        {
            std::cerr << "fioMkdir error: create_directory failed for '" << hostPath
                      << "': " << ec.message() << std::endl;
            setReturnS32(ctx, -1);
        }
        else
        {
            RUNTIME_LOG("fioMkdir: Created directory '" << hostPath << "'");
            setReturnS32(ctx, 0); // Success
        }
    }

    void fioChdir(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        uint32_t pathAddr = getRegU32(ctx, 4); // $a0
        const char *ps2Path = reinterpret_cast<const char *>(getConstMemPtr(rdram, pathAddr));
        if (!ps2Path)
        {
            std::cerr << "fioChdir error: Invalid path address" << std::endl;
            setReturnS32(ctx, -1);
            return;
        }

        std::string hostPath = translatePs2Path(ps2Path);
        if (hostPath.empty())
        {
            std::cerr << "fioChdir error: Failed to translate path '" << ps2Path << "'" << std::endl;
            setReturnS32(ctx, -1);
            return;
        }

        std::error_code ec;
        std::filesystem::current_path(hostPath, ec);

        if (ec)
        {
            std::cerr << "fioChdir error: current_path failed for '" << hostPath
                      << "': " << ec.message() << std::endl;
            setReturnS32(ctx, -1);
        }
        else
        {
            RUNTIME_LOG("fioChdir: Changed directory to '" << hostPath << "'");
            setReturnS32(ctx, 0); // Success
        }
    }

    void fioRmdir(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        uint32_t pathAddr = getRegU32(ctx, 4); // $a0
        const char *ps2Path = reinterpret_cast<const char *>(getConstMemPtr(rdram, pathAddr));
        if (!ps2Path)
        {
            std::cerr << "fioRmdir error: Invalid path address" << std::endl;
            setReturnS32(ctx, -1);
            return;
        }
        std::string hostPath = translatePs2Path(ps2Path);
        if (hostPath.empty())
        {
            std::cerr << "fioRmdir error: Failed to translate path '" << ps2Path << "'" << std::endl;
            setReturnS32(ctx, -1);
            return;
        }

        std::error_code ec;
        bool success = std::filesystem::remove(hostPath, ec);

        if (!success || ec)
        {
            std::cerr << "fioRmdir error: remove failed for '" << hostPath
                      << "': " << ec.message() << std::endl;
            setReturnS32(ctx, -1);
        }
        else
        {
            RUNTIME_LOG("fioRmdir: Removed directory '" << hostPath << "'");
            setReturnS32(ctx, 0); // Success
        }
    }

    void fioGetstat(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        // we wont implement this for now.
        uint32_t pathAddr = getRegU32(ctx, 4);    // $a0
        uint32_t statBufAddr = getRegU32(ctx, 5); // $a1

        const char *ps2Path = reinterpret_cast<const char *>(getConstMemPtr(rdram, pathAddr));
        uint8_t *ps2StatBuf = getMemPtr(rdram, statBufAddr);

        if (!ps2Path)
        {
            std::cerr << "fioGetstat error: Invalid path addr" << std::endl;
            setReturnS32(ctx, -1);
            return;
        }
        if (!ps2StatBuf)
        {
            std::cerr << "fioGetstat error: Invalid buffer addr" << std::endl;
            setReturnS32(ctx, -1);
            return;
        }

        std::string hostPath = translatePs2Path(ps2Path);
        if (hostPath.empty())
        {
            std::cerr << "fioGetstat error: Bad path translate" << std::endl;
            setReturnS32(ctx, -1);
            return;
        }

        setReturnS32(ctx, -1);
    }

    void fioRemove(uint8_t *rdram, R5900Context *ctx, PS2Runtime *runtime)
    {
        uint32_t pathAddr = getRegU32(ctx, 4); // $a0
        const char *ps2Path = reinterpret_cast<const char *>(getConstMemPtr(rdram, pathAddr));
        if (!ps2Path)
        {
            std::cerr << "fioRemove error: Invalid path" << std::endl;
            setReturnS32(ctx, -1);
            return;
        }

        std::string hostPath = translatePs2Path(ps2Path);
        if (hostPath.empty())
        {
            std::cerr << "fioRemove error: Path translate fail" << std::endl;
            setReturnS32(ctx, -1);
            return;
        }

        std::error_code ec;
        bool success = std::filesystem::remove(hostPath, ec);

        if (!success || ec)
        {
            std::cerr << "fioRemove error: remove failed for '" << hostPath
                      << "': " << ec.message() << std::endl;
            setReturnS32(ctx, -1);
        }
        else
        {
            RUNTIME_LOG("fioRemove: Removed file '" << hostPath << "'");
            setReturnS32(ctx, 0); // Success
        }
    }
}
