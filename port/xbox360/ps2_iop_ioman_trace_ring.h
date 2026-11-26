#pragma once
#include "ps2_iop_ioman.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
namespace ps2::xbox360::iop {
class IomanTraceRing final : public IomanTraceSink {
public:
 static constexpr std::size_t Capacity=256, PathCapacity=128;
 struct Record { IomanCommand command{}; std::array<char,PathCapacity> path{}; std::uint16_t pathLength{}; std::int32_t descriptor{-1}; std::uint32_t arg1{},arg2{}; std::int32_t result{}; };
 void onIomanTrace(const IomanTraceRecord &event) noexcept override {
  Record r{}; r.command=event.command; const auto n=event.path.size()<PathCapacity-1?event.path.size():PathCapacity-1;
  for(std::size_t i=0;i<n;++i) r.path[i]=event.path[i];
  r.pathLength=static_cast<std::uint16_t>(n); r.descriptor=event.descriptor; r.arg1=event.arg1; r.arg2=event.arg2; r.result=event.result;
  records_[writeIndex_]=r; writeIndex_=(writeIndex_+1)%Capacity; if(count_<Capacity)++count_;else ++dropped_;
 }
 [[nodiscard]] std::size_t size() const noexcept{return count_;}
 [[nodiscard]] std::uint64_t dropped() const noexcept{return dropped_;}
 [[nodiscard]] const Record &at(std::size_t i) const noexcept {const auto oldest=(writeIndex_+Capacity-count_)%Capacity;return records_[(oldest+i)%Capacity];}
 void clear() noexcept {writeIndex_=count_=0;dropped_=0;}
 [[nodiscard]] static std::string_view operationName(IomanCommand c) noexcept {switch(c){case IomanCommand::Open:return "open";case IomanCommand::Close:return "close";case IomanCommand::Read:return "read";case IomanCommand::Lseek:return "lseek";}return "unknown";}
private: std::array<Record,Capacity> records_{};std::size_t writeIndex_{},count_{};std::uint64_t dropped_{};
};
} // namespace ps2::xbox360::iop
