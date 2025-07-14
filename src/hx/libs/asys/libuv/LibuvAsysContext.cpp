#include <hxcpp.h>
#include <hx/asys/libuv/LibuvAsysContext.h>
#include "BaseData.h"
//#include "system/LibuvCurrentProcess.h"

#include <memory>
#include <thread>

namespace
{
    static std::atomic<::hx::asys::libuv::LibuvAsysContext_obj*> global = nullptr;
    static std::latch latch(1);
}

void hx::asys::Context_obj::boot()
{
    global.store(new (::hx::NewObjectType::NewObjConst) ::hx::asys::libuv::LibuvAsysContext_obj());

    latch.wait();
}

hx::asys::Context hx::asys::Context_obj::get()
{
    return ::hx::asys::libuv::LibuvAsysContext(global.load());
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

hx::asys::libuv::LibuvAsysContext_obj::LibuvAsysContext_obj(/*, hx::asys::system::CurrentProcess _process */)
    // : hx::asys::Context_obj()
    : ctx(new Ctx()) {}

hx::asys::libuv::RootedCallbacks::RootedCallbacks(Dynamic _cbSuccess, Dynamic _cbFailure)
    : cbSuccess(_cbSuccess.mPtr)
    , cbFailure(_cbFailure.mPtr) {}
