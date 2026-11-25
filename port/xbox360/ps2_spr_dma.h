#pragma once
#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <span>
namespace ps2::xbox360::dmac {
inline constexpr std::uint32_t kControllerControl=0x1000E000u;
inline constexpr std::uint32_t kControllerStatus=0x1000E010u;
inline constexpr std::uint32_t kStallAddress=0x1000E060u;
inline constexpr std::uint32_t kScratchpadToRdramChannel=0x1000D000u;
inline constexpr std::uint32_t kRdramToScratchpadChannel=0x1000D400u;
inline constexpr std::uint32_t kScratchpadSize=16u*1024u;
inline constexpr std::uint32_t kDmaEnable=1u;
inline constexpr std::uint32_t kStart=1u<<8u;
enum class Channel : std::uint8_t { ScratchpadToRdram, RdramToScratchpad };
enum class WriteStatus : std::uint8_t { Stored, TransferCompleted, DmaDisabled, UnsupportedMode, InvalidMemory, UnsupportedRegister };
struct WriteResult { WriteStatus status{WriteStatus::UnsupportedRegister}; std::uint32_t cause{}; std::uint32_t bytesTransferred{}; };
struct ChannelRegisters { std::uint32_t chcr{}, madr{}, qwc{}, sadr{}; };
struct ControllerState {
    std::uint32_t ctrl{kDmaEnable}, dstat{}, stadr{};
    ChannelRegisters scratchpadToRdram{}, rdramToScratchpad{};
};
[[nodiscard]] inline ChannelRegisters &channelRegisters(ControllerState &s, Channel c) noexcept {
    return c==Channel::ScratchpadToRdram?s.scratchpadToRdram:s.rdramToScratchpad;
}
[[nodiscard]] inline const ChannelRegisters &channelRegisters(const ControllerState &s, Channel c) noexcept {
    return c==Channel::ScratchpadToRdram?s.scratchpadToRdram:s.rdramToScratchpad;
}
inline void writeDstat(ControllerState &s,std::uint32_t value) noexcept {
    std::uint32_t status=s.dstat&0x3FFu, mask=(s.dstat>>16u)&0x3FFu;
    status &= ~(value&0x3FFu);
    mask ^= (value>>16u)&0x3FFu;
    s.dstat &= ~((0x3FFu)|(0x3FFu<<16u)|(1u<<31u));
    s.dstat |= status|(mask<<16u);
    if((status&mask)!=0u) s.dstat |= 1u<<31u;
}
[[nodiscard]] inline bool readRegister(const ControllerState &s,std::uint32_t address,std::uint32_t &value) noexcept {
    if(address==kControllerControl){value=s.ctrl;return true;}
    if(address==kControllerStatus){value=s.dstat;return true;}
    if(address==kStallAddress){value=s.stadr;return true;}
    for(const auto c:{Channel::ScratchpadToRdram,Channel::RdramToScratchpad}){
        const auto base=c==Channel::ScratchpadToRdram?kScratchpadToRdramChannel:kRdramToScratchpadChannel;
        const auto &r=channelRegisters(s,c);
        if(address==base)value=r.chcr;
        else if(address==base+0x10u)value=r.madr;
        else if(address==base+0x20u)value=r.qwc;
        else if(address==base+0x80u)value=r.sadr;
        else continue;
        return true;
    }
    return false;
}
[[nodiscard]] inline WriteResult startTransfer(ControllerState &s,ChannelRegisters &r,Channel c,
    std::uint32_t chcr,std::span<std::uint8_t> ram,std::span<std::uint8_t> spr) noexcept {
    r.chcr=chcr;
    if((chcr&kStart)==0u)return {WriteStatus::Stored,0u,0u};
    if((s.ctrl&kDmaEnable)==0u)return {WriteStatus::DmaDisabled,0u,0u};
    if((chcr&0xCu)!=0u||(s.ctrl&0xCu)!=0u)return {WriteStatus::UnsupportedMode,0u,0u};
    const std::uint32_t ramOffset=r.madr&0x1FFFFFF0u;
    const std::uint32_t byteCount=(r.qwc&0xFFFFu)*16u;
    std::uint32_t sprOffset=r.sadr&0x3FF0u;
    if((r.madr&0x80000000u)!=0u||spr.size()!=kScratchpadSize||
       static_cast<std::uint64_t>(ramOffset)>ram.size()||
       static_cast<std::uint64_t>(byteCount)>ram.size()-ramOffset)
        return {WriteStatus::InvalidMemory,0u,0u};
    std::uint32_t moved=0u;
    while(moved<byteCount){
        const std::uint32_t chunk=std::min(byteCount-moved,kScratchpadSize-sprOffset);
        auto ramPart=ram.subspan(static_cast<std::size_t>(ramOffset+moved),chunk);
        auto sprPart=spr.subspan(sprOffset,chunk);
        if(c==Channel::ScratchpadToRdram)std::copy(sprPart.begin(),sprPart.end(),ramPart.begin());
        else std::copy(ramPart.begin(),ramPart.end(),sprPart.begin());
        moved+=chunk;
        sprOffset=(sprOffset+chunk)&0x3FFFu;
    }
    r.madr=(r.madr&~0xFu)+byteCount;
    r.sadr=sprOffset;r.qwc=0u;r.chcr=chcr&~kStart;
    if(c==Channel::ScratchpadToRdram&&((s.ctrl>>4u)&0x3u)==2u)s.stadr=r.madr;
    const std::uint32_t cause=c==Channel::ScratchpadToRdram?8u:9u;
    s.dstat|=1u<<cause;
    if(((s.dstat&0x3FFu)&((s.dstat>>16u)&0x3FFu))!=0u)s.dstat|=1u<<31u;
    else s.dstat&=~(1u<<31u);
    return {WriteStatus::TransferCompleted,cause,byteCount};
}
[[nodiscard]] inline WriteResult writeRegister(ControllerState &s,std::span<std::uint8_t> ram,
    std::span<std::uint8_t> spr,std::uint32_t address,std::uint32_t value) noexcept {
    if(address==kControllerControl){s.ctrl=value;return {WriteStatus::Stored,0u,0u};}
    if(address==kControllerStatus){writeDstat(s,value);return {WriteStatus::Stored,0u,0u};}
    if(address==kStallAddress){s.stadr=value;return {WriteStatus::Stored,0u,0u};}
    if(address==kScratchpadToRdramChannel)
        return startTransfer(s,s.scratchpadToRdram,Channel::ScratchpadToRdram,value,ram,spr);
    if(address==kRdramToScratchpadChannel)
        return startTransfer(s,s.rdramToScratchpad,Channel::RdramToScratchpad,value,ram,spr);
    for(const auto c:{Channel::ScratchpadToRdram,Channel::RdramToScratchpad}){
        const auto base=c==Channel::ScratchpadToRdram?kScratchpadToRdramChannel:kRdramToScratchpadChannel;
        auto &r=channelRegisters(s,c);
        if(address==base+0x10u){r.madr=value&0x7FFFFFFFu;return {WriteStatus::Stored,0u,0u};}
        if(address==base+0x20u){r.qwc=value;return {WriteStatus::Stored,0u,0u};}
        if(address==base+0x80u){r.sadr=value;return {WriteStatus::Stored,0u,0u};}
    }
    return {WriteStatus::UnsupportedRegister,0u,0u};
}
} // namespace ps2::xbox360::dmac
