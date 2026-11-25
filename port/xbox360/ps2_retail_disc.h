#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <span>
#include <string>
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
        if (!std::filesystem::is_regular_file(path, error))
            return error ? FileStatus::IoError : FileStatus::NotFound;

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
            if (character == ':')
                return false;
        }

        const std::filesystem::path relative(normalized);
        if (relative.is_absolute() || relative.has_root_name() ||
            relative.has_root_directory())
            return false;
        for (const auto &part : relative)
        {
            if (part == "." || part == "..")
                return false;
        }

        result = (root_ / relative).lexically_normal();
        return true;
    }

    std::filesystem::path root_;
};
} // namespace ps2::xbox360::disc
