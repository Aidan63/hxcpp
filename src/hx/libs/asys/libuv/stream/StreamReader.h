#pragma once

#include <hxcpp.h>
#include <deque>
#include <array>
#include "../LibuvUtils.h"

HX_DECLARE_CLASS4(hx, asys, libuv, stream, StreamReader)

namespace hx::asys::libuv::stream
{
    class StreamReader_obj : public hx::asys::Readable_obj
    {
    public:
        struct QueuedRead final : public BaseRequest
        {
            hx::RootedObject<Array_obj<uint8_t>> array;
            int offset;
            int length;

            QueuedRead(std::unique_ptr<hx::asys::libuv::RootedCallbacks> _callbacks, const Array<uint8_t> _array, const int _offset, const int _length);
        };

        struct Ctx
        {
            uv_stream_t* stream;
            std::deque<QueuedRead> queue;
            std::vector<std::vector<char>> staging;
            std::vector<char> buffer;

            uv_alloc_cb cbAlloc;
            uv_read_cb cbRead;

            Ctx(uv_stream_t* _stream, uv_alloc_cb _cbAlloc, uv_read_cb _cbRead);

            void consume();
            void reject(int code);
        };

        Ctx& ctx;

        StreamReader_obj(Ctx& ctx);

        void read(Array<uint8_t> output, int offset, int length, Dynamic cbSuccess, Dynamic cbFailure) override;
    };
}