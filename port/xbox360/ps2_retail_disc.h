#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace ps2::xbox360::disc
{
enum class FileStatus : std::uint8_t
{
    Ok,
    NotFound,
    InvalidPath,
    OutOfRange,
    IoError
};

// Read-only view of a user-supplied extraction of the retail PS2 disc.
// The root is supplied by the caller (for example, an HDD or USB directory);
// no copyrighted game data is embedded in the port.
class RetailDisc
{
public:
    explicit RetailDisc(std::filesystem::path root)
        : root_(std::move(root))
    {
    }

    [[nodiscard]] const std::filesystem::path &root() const noexcept
    {
        return root_;
    }

    [[nodiscard]] FileStatus fileSize(const std::string &relativePath,
                                      std::uint64_t &size) const
    {
        std::filesystem::path path;
        if (!resolve(relativePath, path))
            return FileStatus::InvalidPath;

        std::error_code error;
        const auto entryStatus = std::filesystem::status(path, error);
        if (error == std::errc::no_such_file_or_directory ||
            error == std::errc::not_a_directory ||
            entryStatus.type() == std::filesystem::file_type::not_found)
            return FileStatus::NotFound;
        if (error)
            return FileStatus::IoError;
        if (!std::filesystem::is_regular_file(entryStatus))
            return FileStatus::NotFound;

        const auto value = std::filesystem::file_size(path, error);
        if (error)
            return FileStatus::IoError;
        size = static_cast<std::uint64_t>(value);
        return FileStatus::Ok;
    }

    [[nodiscard]] bool exists(const std::string &relativePath) const
    {
        std::uint64_t ignored{};
        return fileSize(relativePath, ignored) == FileStatus::Ok;
    }

    [[nodiscard]] FileStatus read(const std::string &relativePath,
                                  std::uint64_t offset,
                                  std::span<std::uint8_t> destination) const
    {
        std::filesystem::path path;
        if (!resolve(relativePath, path))
            return FileStatus::InvalidPath;

        std::uint64_t size{};
        const auto status = fileSize(relativePath, size);
        if (status != FileStatus::Ok)
            return status;
        if (offset > size || destination.size() > size - offset)
            return FileStatus::OutOfRange;

        std::ifstream input(path, std::ios::binary);
        if (!input)
            return FileStatus::IoError;
        if (offset > static_cast<std::uint64_t>(
                         std::numeric_limits<std::streamoff>::max()))
            return FileStatus::OutOfRange;

        input.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
        if (!input)
            return FileStatus::IoError;
        if (!destination.empty())
        {
            input.read(reinterpret_cast<char *>(destination.data()),
                       static_cast<std::streamsize>(destination.size()));
            if (input.gcount() != static_cast<std::streamsize>(destination.size()))
                return FileStatus::IoError;
        }
        return FileStatus::Ok;
    }

    [[nodiscard]] FileStatus readAll(const std::string &relativePath,
                                     std::vector<std::uint8_t> &destination) const
    {
        std::uint64_t size{};
        const auto status = fileSize(relativePath, size);
        if (status != FileStatus::Ok)
            return status;
        if (size > destination.max_size() ||
            size > static_cast<std::uint64_t>(
                       std::numeric_limits<std::size_t>::max()))
            return FileStatus::OutOfRange;

        std::vector<std::uint8_t> result(static_cast<std::size_t>(size));
        const auto readStatus = read(relativePath, 0u, result);
        if (readStatus != FileStatus::Ok)
            return readStatus;
        destination = std::move(result);
        return FileStatus::Ok;
    }

private:
    [[nodiscard]] bool resolve(const std::string &relativePath,
                               std::filesystem::path &result) const
    {
        if (relativePath.empty())
            return false;

        std::string normalized = relativePath;
        for (auto &character : normalized)
        {
            if (character == '\\')
                character = '/';
        }

        // PS2 file APIs commonly pass paths such as
        // "cdrom0:\\IOP\\MODULE.IRX;1". Strip only the optical-disc
        // device prefix; never accept arbitrary host drive/device paths.
        const auto startsWithAsciiInsensitive = [](std::string_view value,
                                                   std::string_view prefix)
        {
            if (value.size() < prefix.size())
                return false;
            for (std::size_t i = 0; i < prefix.size(); ++i)
            {
                const auto lower = [](unsigned char c)
                {
                    return c >= 'A' && c <= 'Z'
                               ? static_cast<unsigned char>(c - 'A' + 'a')
                               : c;
                };
                if (lower(static_cast<unsigned char>(value[i])) !=
                    lower(static_cast<unsigned char>(prefix[i])))
                    return false;
            }
            return true;
        };
        const bool hasDiscDevice =
            startsWithAsciiInsensitive(normalized, "cdrom0:") ||
            startsWithAsciiInsensitive(normalized, "cdrom1:");
        if (hasDiscDevice)
        {
            normalized.erase(0u, 7u);
        }
        else if (!normalized.empty() && normalized.front() == '/')
        {
            // A leading slash without a PS2 optical-disc device is an
            // absolute host path and must never be rebased under the disc.
            return false;
        }

        const std::size_t version = normalized.find_last_of(';');
        if (version != std::string::npos && version + 1u < normalized.size())
        {
            bool numericVersion = true;
            for (std::size_t i = version + 1u; i < normalized.size(); ++i)
            {
                if (normalized[i] < '0' || normalized[i] > '9')
                {
                    numericVersion = false;
                    break;
                }
            }
            if (numericVersion)
                normalized.erase(version);
        }

        if (hasDiscDevice)
        {
            while (!normalized.empty() && normalized.front() == '/')
                normalized.erase(normalized.begin());
        }
        if (normalized.empty())
            return false;
        if (normalized.find(':') != std::string::npos)
            return false;

        const std::filesystem::path relative(normalized);
        if (relative.is_absolute() || relative.has_root_name() ||
            relative.has_root_directory())
            return false;
        for (const auto &part : relative)
        {
            if (part == "." || part == "..")
                return false;
        }

        // Resolve components against the extracted tree using ASCII
        // case-insensitive matching. PS2 paths are commonly case-insensitive,
        // while extraction hosts (Linux in particular) may not be.
        std::filesystem::path current = root_;
        for (const auto &part : relative)
        {
            std::error_code directoryError;
            if (!std::filesystem::is_directory(current, directoryError))
            {
                current /= part;
                continue;
            }

            const auto equalInsensitive = [](std::string_view lhs, std::string_view rhs)
            {
                if (lhs.size() != rhs.size())
                    return false;
                for (std::size_t i = 0; i < lhs.size(); ++i)
                {
                    const auto lower = [](unsigned char c)
                    {
                        return c >= 'A' && c <= 'Z'
                                   ? static_cast<unsigned char>(c - 'A' + 'a')
                                   : c;
                    };
                    if (lower(static_cast<unsigned char>(lhs[i])) !=
                        lower(static_cast<unsigned char>(rhs[i])))
                        return false;
                }
                return true;
            };

            bool matched = false;
            std::error_code iterationError;
            for (std::filesystem::directory_iterator it(current, iterationError), end;
                 !iterationError && it != end; it.increment(iterationError))
            {
                if (equalInsensitive(it->path().filename().string(), part.string()))
                {
                    current = it->path();
                    matched = true;
                    break;
                }
            }
            if (!matched)
                current /= part;
        }

        result = current.lexically_normal();
        return true;
    }

    std::filesystem::path root_;
};
} // namespace ps2::xbox360::disc
