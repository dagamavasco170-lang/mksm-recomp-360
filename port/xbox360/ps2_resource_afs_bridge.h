#pragma once
#include "ps2_iop_afs_stream.h"
#include "ps2_resource_loading_runtime.h"
#include <cerrno>
#include <cstdint>
#include <span>
#include <string>

namespace ps2::xbox360::resources {
// Adapter joining the existing AFS stream I/O implementation to the loading runtime.
// One adapter is configured for a single archive; entry indices remain AFS entry IDs.
class AfsResourceBridge {
public:
    AfsResourceBridge(iop::AfsStreamIo& afs,std::string archive):afs_(afs),archive_(std::move(archive)){}
    [[nodiscard]] bool loadBlob(std::uint32_t entry,ResourceLoadingRuntime::Blob&out){
        const auto fd=afs_.open(path(entry));if(fd<0)return false;
        const auto end=afs_.seek(fd,0,iop::SeekWhence::End);
        if(end<0||afs_.seek(fd,0,iop::SeekWhence::Set)<0){afs_.close(fd);return false;}
        out.resize(static_cast<std::size_t>(end));std::size_t offset=0;bool ok=true;
        while(offset<out.size()){
            const auto n=afs_.read(fd,std::span<std::uint8_t>(out).subspan(offset));
            if(n<=0){ok=false;break;}offset+=static_cast<std::size_t>(n);
        }
        afs_.close(fd);if(!ok)out.clear();return ok;
    }
    [[nodiscard]] std::int32_t readChunk(std::uint32_t entry,std::uint64_t offset,std::span<std::uint8_t>out){
        const auto fd=afs_.open(path(entry));if(fd<0)return fd;
        const auto pos=afs_.seek(fd,static_cast<std::int32_t>(offset),iop::SeekWhence::Set);
        const auto n=pos<0?pos:afs_.read(fd,out);afs_.close(fd);return n;
    }
    [[nodiscard]] const ResourceLoadingRuntime::Loader loader(std::uint32_t entry){
        return [this,entry](ResourceLoadingRuntime::ResourceId,ResourceLoadingRuntime::Blob&out){return loadBlob(entry,out);};
    }
    [[nodiscard]] const ResourceLoadingRuntime::StreamReader streamReader(std::uint32_t entry){
        return [this,entry](ResourceLoadingRuntime::ResourceId,std::uint64_t offset,std::span<std::uint8_t>out){return readChunk(entry,offset,out);};
    }
    [[nodiscard]] std::string virtualPath(std::uint32_t entry)const{return path(entry);}
private:
    [[nodiscard]] std::string path(std::uint32_t entry)const{return "afs0:/"+archive_+"/"+std::to_string(entry);}
    iop::AfsStreamIo&afs_;std::string archive_;
};
} // namespace ps2::xbox360::resources
