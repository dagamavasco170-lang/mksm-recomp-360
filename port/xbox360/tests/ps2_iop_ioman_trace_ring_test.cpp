#include "ps2_iop_ioman_trace_ring.h"
#include <cassert>
#include <string>
int main() {
 using namespace ps2::xbox360::iop; IomanTraceRing ring;
 assert(ring.size()==0);
 ring.onIomanTrace({IomanCommand::Open,"afs0:/GAMEDATA.WAG/5",31,1,0,31});
 ring.onIomanTrace({IomanCommand::Read,{},31,0x2000,512,512});
 assert(ring.size()==2 && ring.at(0).command==IomanCommand::Open);
 assert(std::string(ring.at(0).path.data(),ring.at(0).pathLength)=="afs0:/GAMEDATA.WAG/5");
 assert(ring.at(1).descriptor==31 && ring.at(1).arg2==512);
 assert(IomanTraceRing::operationName(IomanCommand::Lseek)=="lseek");
 for(std::size_t i=0;i<IomanTraceRing::Capacity+7;++i) ring.onIomanTrace({IomanCommand::Close,{},static_cast<int>(i),0,0,0});
 assert(ring.size()==IomanTraceRing::Capacity && ring.dropped()==9);
 assert(ring.at(0).command==IomanCommand::Close);
 ring.clear(); assert(ring.size()==0 && ring.dropped()==0); return 0;
}
