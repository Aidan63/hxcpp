#include <hxcpp.h>
#include "LibuvAsysContext.h"
#include "BaseData.h"
#include "system/LibuvCurrentProcess.h"

#include <memory>
#include <thread>


hx::asys::Context hx::asys::Context_obj::get()
{
    auto obj = ::hx::asys::libuv::LibuvAsysContext(new ::hx::asys::libuv::LibuvAsysContext_obj());

    obj->ctx->latch.wait();

    return obj;
}

hx::asys::libuv::LibuvAsysContext_obj::Ctx::Ctx()
    : loop()
    , serialised()
    , ttys()
    , lock()
    , queue()
    , latch(1)
    , thread(&hx::asys::libuv::LibuvAsysContext_obj::Ctx::run, this)
{
    loop.data = this;
    serialised.data = this;
}

void hx::asys::libuv::LibuvAsysContext_obj::Ctx::run()
{
    HX_TOP_OF_STACK;

    auto result = int{ 0 };

    if ((result = uv_loop_init(&loop) < 0))
    {
        hx::CriticalError(String::create(uv_strerror(result)));
    }
    if ((result = uv_async_init(&loop, &serialised, consume) < 0))
    {
        hx::CriticalError(String::create(uv_strerror(result)));
    }
    for (auto i = 0; i < ttys.size(); i++)
    {
        if ((result = uv_tty_init(&loop, &ttys.at(i), i, false)) < 0)
        {
            hx::CriticalError(String::create(uv_strerror(result)));
        }
    }

    latch.count_down();

    hx::EnterGCFreeZone();

    if (uv_run(&loop, UV_RUN_DEFAULT) > 0)
    {
        // Cleanup the loop according to https://stackoverflow.com/a/25831688
        uv_walk(
            &loop,
            [](uv_handle_t* handle, void*) {
                uv_close(handle, nullptr);
            },
            nullptr);

        if (uv_run(&loop, UV_RUN_DEFAULT) > 0)
        {
            hx::ExitGCFreeZone();
            hx::CriticalError(String::create("Failed to remove all active handles"));
        }
    }

    if ((result = uv_loop_close(&loop)) < 0)
    {
        hx::ExitGCFreeZone();
        hx::CriticalError(String::create(uv_strerror(result)));
    }

    hx::ExitGCFreeZone();
}

void hx::asys::libuv::LibuvAsysContext_obj::Ctx::consume(uv_async_t* async)
{
    auto ctx   = static_cast<Ctx*>(async->data);
    auto guard = std::lock_guard(ctx->lock);

    while (ctx->queue.empty() == false)
    {
        auto& work = ctx->queue.front();

        work->run(&ctx->loop);

        ctx->queue.pop_front();
    }
}

hx::asys::libuv::LibuvAsysContext_obj::LibuvAsysContext_obj()
    : ctx(new Ctx())
{
    process =
        hx::asys::system::CurrentProcess(new hx::asys::libuv::system::LibuvCurrentProcess(
            new hx::asys::libuv::system::LibuvCurrentProcess::Ctx(reinterpret_cast<uv_stream_t*>(&ctx->ttys.at(0))),
            reinterpret_cast<uv_stream_t*>(&ctx->ttys.at(1)),
            reinterpret_cast<uv_stream_t*>(&ctx->ttys.at(2))));
}
