#pragma once

#include <hx/asys/Asys.h>
#include <hx/Thread.h>
#include <uv.h>
#include <thread>
#include <mutex>
#include <deque>
#include <array>

HX_DECLARE_CLASS3(hx, asys, libuv, LibuvAsysContext)

namespace hx::asys::libuv
{
    struct BaseRequest
    {
        hx::RootedObject<hx::Object> cbSuccess;
        hx::RootedObject<hx::Object> cbFailure;

        BaseRequest(Dynamic _cbSuccess, Dynamic _cbFailure);
        virtual ~BaseRequest() = default;

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
            std::deque<std::unique_ptr<hx::asys::libuv::BaseRequest>> queue;

            void run();
            static void consume(uv_async_t*);

            Ctx();
        };

        Ctx* ctx;

        LibuvAsysContext_obj(Ctx* ctx);

        void enqueue(std::unique_ptr<hx::asys::libuv::BaseRequest> request);
    };
}