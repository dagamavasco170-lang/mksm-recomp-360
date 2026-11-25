#include "ps2_spr_dma.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <vector>
namespace {
using namespace ps2::xbox360::dmac;
void require(bool ok,const char *msg){if(!ok)throw std::runtime_error(msg);}
std::uint32_t read(const ControllerState &s,std::uint32_t a){std::uint32_t v=0;require(readRegister(s,a,v),"register read");return v;}
void store(ControllerState &s,std::span<std::uint8_t> ram,std::span<std::uint8_t> spr,std::uint32_t a,std::uint32_t v){
    require(writeRegister(s,ram,spr,a,v).status==WriteStatus::Stored,"register write");
}
}
int main(){
    using namespace ps2::xbox360::dmac;
    constexpr std::uint32_t ramOffset=0x80000u,sprStart=0x3FF0u;
    constexpr std::array<std::uint32_t,3> aliases{0u,0x80000000u,0xA0000000u};
    constexpr std::array<std::uint32_t,5> qwcValues{0u,2u,0x14u,0x400u,0x801u};
    std::vector<std::uint8_t> ram(1024u*1024u,0u);
    std::array<std::uint8_t,kScratchpadSize> spr{};
    std::uint32_t cases=0u;
    for(const auto alias:aliases)for(const auto c:{Channel::ScratchpadToRdram,Channel::RdramToScratchpad})for(const auto qwc:qwcValues){
        ControllerState state{};
        std::fill(ram.begin(),ram.end(),0xA5u);
        for(std::uint32_t i=0;i<kScratchpadSize;++i)spr[i]=static_cast<std::uint8_t>(i*37u+11u);
        for(std::uint32_t i=0;i<qwc*16u;++i)ram[ramOffset+i]=static_cast<std::uint8_t>(i*19u+(i>>14u));
        const auto originalSpr=spr;auto expectedSpr=originalSpr;
        std::vector<std::uint8_t> expectedRam(ram.begin()+ramOffset,ram.begin()+ramOffset+qwc*16u);
        for(std::uint32_t i=0;i<qwc*16u;++i){
            const auto index=(sprStart+i)&0x3FFFu;
            if(c==Channel::ScratchpadToRdram)expectedRam[i]=originalSpr[index];
            else expectedSpr[index]=ram[ramOffset+i];
        }
        store(state,ram,spr,kControllerControl,0x21u);
        const auto base=c==Channel::ScratchpadToRdram?kScratchpadToRdramChannel:kRdramToScratchpadChannel;
        store(state,ram,spr,base+0x10u,alias|ramOffset);
        store(state,ram,spr,base+0x20u,qwc);
        store(state,ram,spr,base+0x80u,sprStart);
        const auto result=writeRegister(state,ram,spr,base,kStart);
        require(result.status==WriteStatus::TransferCompleted,"normal SPR transfer completes");
        require(result.bytesTransferred==qwc*16u,"transferred byte count");
        const auto storedMadr=(alias|ramOffset)&0x7FFFFFFFu;
        const auto expectedMadrAfter=(storedMadr&~0xFu)+qwc*16u;
        require(read(state,base+0x10u)==expectedMadrAfter,"MADR advances");
        require(read(state,base+0x20u)==0u,"QWC clears");
        require(read(state,base+0x80u)==((sprStart+qwc*16u)&0x3FFFu),"SADR wraps at 16 KiB");
        require((read(state,base)&kStart)==0u,"CHCR start clears");
        require((read(state,kControllerStatus)&(1u<<(c==Channel::ScratchpadToRdram?8u:9u)))!=0u,"completion cause rises");
        require(ram[ramOffset-1u]==0xA5u&&ram[ramOffset+qwc*16u]==0xA5u,"RAM guards remain unchanged");
        if(c==Channel::ScratchpadToRdram){
            require(std::equal(expectedRam.begin(),expectedRam.end(),ram.begin()+ramOffset),"SPR-to-RDRAM payload");
            require(spr==originalSpr,"SPR source remains unchanged");
            require(read(state,kStallAddress)==expectedMadrAfter,"D_STADR updates in stall-control mode");
        }else{
            require(spr==expectedSpr,"RDRAM-to-SPR payload");
            require(read(state,kStallAddress)==0u,"RDRAM-to-SPR does not update D_STADR");
        }
        ++cases;
    }
    {
        ControllerState state{};state.ctrl=0u;
        store(state,ram,spr,kScratchpadToRdramChannel+0x10u,ramOffset);
        store(state,ram,spr,kScratchpadToRdramChannel+0x20u,2u);
        const auto result=writeRegister(state,ram,spr,kScratchpadToRdramChannel,kStart);
        require(result.status==WriteStatus::DmaDisabled,"disabled DMA stays incomplete");
        require(read(state,kScratchpadToRdramChannel+0x20u)==2u&&read(state,kControllerStatus)==0u,"disabled QWC and D_STAT remain");
    }
    {
        ControllerState state{};
        store(state,ram,spr,kScratchpadToRdramChannel+0x10u,ramOffset);
        store(state,ram,spr,kScratchpadToRdramChannel+0x20u,2u);
        const auto result=writeRegister(state,ram,spr,kScratchpadToRdramChannel,kStart|4u);
        require(result.status==WriteStatus::UnsupportedMode,"chain/interleave modes rejected");
        require(read(state,kScratchpadToRdramChannel+0x20u)==2u&&read(state,kControllerStatus)==0u,"unsupported mode not completed");
    }
    {
        ControllerState state{};
        store(state,ram,spr,kScratchpadToRdramChannel+0x10u,static_cast<std::uint32_t>(ram.size()-16u));
        store(state,ram,spr,kScratchpadToRdramChannel+0x20u,2u);
        const auto result=writeRegister(state,ram,spr,kScratchpadToRdramChannel,kStart);
        require(result.status==WriteStatus::InvalidMemory,"out-of-range RAM rejected");
        require(read(state,kScratchpadToRdramChannel+0x20u)==2u&&read(state,kControllerStatus)==0u,"invalid transfer not completed");
    }
    {
        ControllerState state{};
        store(state,ram,spr,kScratchpadToRdramChannel+0x10u,ramOffset);
        store(state,ram,spr,kScratchpadToRdramChannel+0x20u,1u);
        store(state,ram,spr,kScratchpadToRdramChannel+0x80u,0u);
        require(writeRegister(state,ram,spr,kScratchpadToRdramChannel,kStart).status==WriteStatus::TransferCompleted,"D_STAT mask setup");
        writeDstat(state,1u<<24u);
        require((read(state,kControllerStatus)&(1u<<31u))!=0u,"D_STAT mask raises IP");
        writeDstat(state,1u<<8u);
        require((read(state,kControllerStatus)&((1u<<31u)|(1u<<8u)))==0u,"D_STAT W1C clears status/IP");
    }
    std::printf("PASS: %u normal SPR DMA alias/direction/QWC cases plus disabled, unsupported, bounds and D_STAT checks\n",static_cast<unsigned>(cases));
    return 0;
}
