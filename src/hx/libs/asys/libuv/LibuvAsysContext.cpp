#include <hxcpp.h>
#include <hx/asys/libuv/LibuvAsysContext.h>
#include "BaseData.h"
//#include "system/LibuvCurrentProcess.h"

#include <memory>
#include <thread>

namespace
{
    hx::asys::libuv::LibuvAsysContext_obj::Ctx* global = nullptr;
    volatile int created = 0;
}

hx::asys::Context hx::asys::Context_obj::create()
{
    if (nullptr == global)
    {
        global = new libuv::LibuvAsysContext_obj::Ctx();
    }

    while (_hx_atomic_load(&created) == 0)
    {
        // 
    }

    return Context(new hx::asys::libuv::LibuvAsysContext_obj(global));
}

hx::asys::libuv::LibuvAsysContext_obj::Ctx::Ctx()
    : loop()
    , serialised()
    , ttys()
    // , reader(reinterpret_cast<uv_stream_t*>(&ttys.at(0)))
    , lock()
    , queue()
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

    _hx_atomic_store(&created, 1);

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
    auto gc    = AutoGCZone();

    while (ctx->queue.empty() == false)
    {
        auto work = ctx->queue.front().release();

        work->run(&ctx->loop);

        ctx->queue.pop_front();
    }
}

hx::asys::libuv::LibuvAsysContext_obj::LibuvAsysContext_obj(Ctx* _ctx /*, hx::asys::system::CurrentProcess _process */)
    // : hx::asys::Context_obj()
    : ctx(_ctx) {}

void hx::asys::libuv::LibuvAsysContext_obj::enqueue(std::unique_ptr<hx::asys::libuv::BaseRequest> request)
{
    auto guard = std::lock_guard(ctx->lock);

    ctx->queue.push_back(std::move(request));

    auto result = uv_async_send(&ctx->serialised);
    if (result < 0)
    {
        hx::CriticalError(String::create(uv_strerror(result)));
    }
}
