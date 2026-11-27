#pragma once

#include "ps2_generated_runtime_abi.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <span>

namespace ps2::xbox360
{
enum class BlockDispatchStatus : std::uint8_t
{
    BudgetExhausted,
    UnknownProgramCounter,
    NullEntry,
    NoProgress
};

struct BlockDispatchResult
{
    BlockDispatchStatus status{BlockDispatchStatus::UnknownProgramCounter};
    std::size_t executedBlocks{};
    std::uint32_t lastPc{};
};

// Sparse PC -> generated function registry. It consumes the exact guest
// addresses emitted by FunctionTableEmitter; it does not infer block ranges.
class RecompiledBlockDispatcher
{
public:
    [[nodiscard]] bool bind(std::uint32_t guestPc,
                            PS2Runtime::RecompiledFunction function)
    {
        if ((guestPc & 3u) != 0u || function == nullptr)
            return false;
        return entries_.emplace(guestPc, function).second;
    }

    [[nodiscard]] bool load(std::span<const PS2GeneratedBlockEntry> entries)
    {
        std::map<std::uint32_t, PS2Runtime::RecompiledFunction> candidate;
        for (const auto &entry : entries)
        {
            if ((entry.guestPc & 3u) != 0u || entry.function == nullptr ||
                !candidate.emplace(entry.guestPc, entry.function).second)
                return false;
        }
        entries_.swap(candidate);
        return true;
    }

    [[nodiscard]] PS2Runtime::RecompiledFunction resolve(std::uint32_t guestPc) const noexcept
    {
        const auto it = entries_.find(guestPc);
        return it == entries_.end() ? nullptr : it->second;
    }

    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }
    void clear() noexcept { entries_.clear(); }

    [[nodiscard]] BlockDispatchResult run(
        std::uint8_t *rdram, R5900Context &context, PS2Runtime *runtime,
        std::size_t maxBlocks) const
    {
        BlockDispatchResult result{};
        result.lastPc = context.pc;
        if (maxBlocks == 0u)
        {
            result.status = BlockDispatchStatus::BudgetExhausted;
            return result;
        }

        while (result.executedBlocks < maxBlocks)
        {
            result.lastPc = context.pc;
            const auto function = resolve(context.pc);
            if (function == nullptr)
            {
                result.status = BlockDispatchStatus::UnknownProgramCounter;
                return result;
            }
            if (rdram == nullptr || runtime == nullptr)
            {
                result.status = BlockDispatchStatus::NullEntry;
                return result;
            }

            const std::uint32_t previousPc = context.pc;
            function(rdram, &context, runtime);
            ++result.executedBlocks;
            result.lastPc = context.pc;
            if (context.pc == previousPc)
            {
                result.status = BlockDispatchStatus::NoProgress;
                return result;
            }
        }

        result.status = BlockDispatchStatus::BudgetExhausted;
        return result;
    }

private:
    std::map<std::uint32_t, PS2Runtime::RecompiledFunction> entries_;
};
} // namespace ps2::xbox360
