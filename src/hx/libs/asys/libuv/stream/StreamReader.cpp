#include <hxcpp.h>
#include "StreamReader.h"

hx::asys::libuv::stream::StreamReader_obj::Ctx::Ctx(uv_stream_t* _stream, uv_alloc_cb _cbAlloc, uv_read_cb _cbRead)
    : stream(_stream)
    , cbAlloc(_cbAlloc)
    , cbRead(_cbRead)
    , queue()
    , staging()
    , buffer()
{
}

void hx::asys::libuv::stream::StreamReader_obj::Ctx::consume()
{
    while (!buffer.empty() && !queue.empty())
    {
        auto& request = queue.front();
        auto size     = std::min(int(buffer.size()), request.length);

        request.array.rooted->memcpy(request.offset, reinterpret_cast<uint8_t*>(buffer.data()), size);

        buffer.erase(buffer.begin(), buffer.begin() + size);

        queue.pop_front();

        Dynamic(request.cbSuccess.rooted)(size);
    }

    if (queue.empty())
    {
        uv_read_stop(stream);
    }
}

void hx::asys::libuv::stream::StreamReader_obj::Ctx::reject(int code)
{
    buffer.clear();

    while (!queue.empty())
    {
        auto& request = queue.front();

        Dynamic(request.cbFailure.rooted)(asys::libuv::uv_err_to_enum(code));

        queue.pop_front();
    }
}

hx::asys::libuv::stream::StreamReader_obj::QueuedRead::QueuedRead(const Array<uint8_t> _array, const int _offset, const int _length, const Dynamic _cbSuccess, const Dynamic _cbFailure)
    : BaseRequest(_cbSuccess, _cbFailure)
    , array(_array.mPtr)
    , offset(_offset)
    , length(_length)
{
}

hx::asys::libuv::stream::StreamReader_obj::StreamReader_obj(Ctx& _ctx)
    : ctx(_ctx) {}

void hx::asys::libuv::stream::StreamReader_obj::read(Array<uint8_t> output, int offset, int length, Dynamic cbSuccess, Dynamic cbFailure)
{
    class ReadWork final : public hx::asys::libuv::CallbackWorkRequest
    {
        Ctx& ctx;
        const hx::RootedObject<Array_obj<uint8_t>> array;
        const int offset;
        const int length;

    public:
        ReadWork(Dynamic _cbSuccess, Dynamic _cbFailure, Ctx& _ctx, Array<uint8_t> _output, int _offset, int _length)
            : CallbackWorkRequest(_cbSuccess, _cbFailure)
            , ctx(_ctx)
            , array(_output.mPtr)
            , offset(_offset)
            , length(_length) {}

        void run(uv_loop_t* loop) override
        {
            auto gcZone = hx::AutoGCZone();

            if (ctx.queue.empty())
            {
                if (!ctx.buffer.empty())
                {
                    ctx.queue.emplace_back(array.rooted, offset, length, callbacks->cbSuccess.rooted, callbacks->cbFailure.rooted);
                    ctx.consume();

                    return;
                }

                auto result = uv_read_start(ctx.stream, ctx.cbAlloc, ctx.cbRead);
                if (result < 0 && result != UV_EALREADY)
                {
                    callbacks->fail(uv_err_to_enum(result));

                    return;
                }
            }

            ctx.queue.emplace_back(array.rooted, offset, length, callbacks->cbSuccess.rooted, callbacks->cbFailure.rooted);
        }
    };

    auto libuv = static_cast<hx::asys::libuv::LibuvAsysContext_obj::Ctx*>(ctx.stream->loop->data);

    libuv->emplace<ReadWork>(cbSuccess, cbFailure, ctx, output, offset, length);
}
