#pragma once

#include <hx/asys/Asys.h>
#include <hx/Thread.h>
#include <uv.h>
#include <thread>
#include <mutex>
#include <deque>
#include <array>
#include <functional>

HX_DECLARE_CLASS3(hx, asys, libuv, LibuvAsysContext)

namespace hx::asys::libuv
{
    struct BaseRequest
    {
        hx::RootedObject<hx::Object> cbSuccess;
        hx::RootedObject<hx::Object> cbFailure;

        BaseRequest(Dynamic _cbSuccess, Dynamic _cbFailure);
        virtual ~BaseRequest() = default;
    };

    struct WorkRequest : public BaseRequest
    {
        WorkRequest(Dynamic _cbSuccess, Dynamic _cbFailure) : BaseRequest(_cbSuccess.mPtr, _cbFailure.mPtr) {}

        virtual void run(uv_loop_t* loop) = 0;
    };

    class LibuvAsysContext_obj final : public Context_obj
    {
    public:
        struct Ctx
        {
            uv_loop_t loop;
            uv_async_t serialised;
            std::array<uv_tty_t, 3> ttys;
            //hx::asys::libuv::stream::StreamReader_obj::Ctx reader;
            std::thread thread;
            std::mutex lock;
            std::deque<std::unique_ptr<WorkRequest>> queue;

            Ctx();

            void enqueue(std::unique_ptr<WorkRequest> request);
            void run();

            static void consume(uv_async_t*);
        };

        Ctx* ctx;

        LibuvAsysContext_obj(Ctx* ctx);
    };
}