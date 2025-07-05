#pragma once

#include <hxcpp.h>
#include <hx/asys/libuv/LibuvAsysContext.h>
#include <atomic>

namespace hx::asys::libuv::filesystem
{
    class LibuvFile_obj final : public hx::asys::filesystem::File_obj
    {
        uv_loop_t* loop;
        std::atomic_bool closed;

    public:
        uv_file file;

        LibuvFile_obj(uv_loop_t* _loop, uv_file _file, const String _path);

        void write(::cpp::Int64 pos, Array<uint8_t> data, int offset, int length, Dynamic cbSuccess, Dynamic cbFailure) override;
        void read(::cpp::Int64 pos, Array<uint8_t> output, int offset, int length, Dynamic cbSuccess, Dynamic cbFailure) override;
        void info(Dynamic cbSuccess, Dynamic cbFailure) override;
        void resize(int size, Dynamic cbSuccess, Dynamic cbFailure) override;
        void setPermissions(int permissions, Dynamic cbSuccess, Dynamic cbFailure) override;
        void setOwner(int user, int group, Dynamic cbSuccess, Dynamic cbFailure) override;
        void setTimes(int accessTime, int modificationTime, Dynamic cbSuccess, Dynamic cbFailure) override;
        void flush(Dynamic cbSuccess, Dynamic cbFailure) override;
        void close(Dynamic cbSuccess, Dynamic cbFailure) override;

        void __Mark(hx::MarkContext* __inCtx) override;
#ifdef HXCPP_VISIT_ALLOCS
        void __Visit(hx::VisitContext* __inCtx) override;
#endif
    };
}