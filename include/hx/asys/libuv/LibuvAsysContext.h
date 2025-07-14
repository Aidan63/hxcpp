#pragma once

#include <hx/asys/Asys.h>
#include <hx/Thread.h>
#include <uv.h>
#include <thread>
#include <mutex>
#include <deque>
#include <array>
#include <functional>
#include <atomic>
#include <latch>

HX_DECLARE_CLASS3(hx, asys, libuv, LibuvAsysContext)

namespace hx::asys::libuv
{
    struct RootedCallbacks final
    {
        hx::RootedObject<hx::Object> cbSuccess;
        hx::RootedObject<hx::Object> cbFailure;

        RootedCallbacks(Dynamic _cbSuccess, Dynamic _cbFailure);

        template <class... TArgs>
        void succeed(TArgs... args)
        {
            Dynamic(cbSuccess.rooted)(args...);
        }

        template <class... TArgs>
        void fail(TArgs... args)
        {
            Dynamic(cbFailure.rooted)(args...);
        }
    };

    struct BaseRequest
    {
        std::unique_ptr<RootedCallbacks> callbacks;

        BaseRequest(std::unique_ptr<RootedCallbacks> _callbacks);
        virtual ~BaseRequest() = default;
    };

    struct WorkRequest
    {
        virtual void run(uv_loop_t* loop) = 0;
    };

    class CallbackWorkRequest : public WorkRequest
    {
    protected:
        std::unique_ptr<RootedCallbacks> callbacks;

    public:
        CallbackWorkRequest(Dynamic _cbSuccess, Dynamic _cbFailure) : callbacks(std::make_unique<RootedCallbacks>(_cbSuccess, _cbFailure)) {}

        virtual ~CallbackWorkRequest() = default;
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

            template<class T, class... TArgs>
            void emplace(TArgs&&... args)
            {
                auto guard = std::lock_guard(lock);

                queue.emplace_back(new T(std::forward<TArgs>(args)...));

                auto result = uv_async_send(&serialised);
                if (result < 0)
                {
                    hx::CriticalError(String::create(uv_strerror(result)));
                }
            }

            void run();

            static void consume(uv_async_t*);
        };

        Ctx* ctx;

        LibuvAsysContext_obj();
    };
}