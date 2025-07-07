#include <hxcpp.h>
#include <array>
#include <memory>
#include "../LibuvUtils.h"
#include "NetUtils.h"

namespace
{
    struct ReverseRequest final : public hx::asys::libuv::BaseRequest
    {
        uv_getnameinfo_t uv;

        ReverseRequest(Dynamic _cbSuccess, Dynamic _cbFailure) : BaseRequest(_cbSuccess, _cbFailure)
        {
            uv.data = this;
        }

        static void callback(uv_getnameinfo_t* request, int status, const char* hostname, const char* service)
        {
            auto gcZone    = hx::AutoGCZone();
            auto spRequest = std::unique_ptr<BaseRequest>(static_cast<BaseRequest*>(request->data));

            if (status < 0)
            {
                Dynamic(spRequest->cbFailure.rooted)(hx::asys::libuv::uv_err_to_enum(status));
            }
            else
            {
                Dynamic(spRequest->cbSuccess.rooted)(String::create(hostname));
            }
        }
    };
}

void hx::asys::net::dns::resolve(Context ctx, String host, Dynamic cbSuccess, Dynamic cbFailure)
{
    class AddrInfoCleaner
    {
    public:
        addrinfo* info;

        AddrInfoCleaner(addrinfo* _info) : info(_info) {}
        ~AddrInfoCleaner()
        {
            uv_freeaddrinfo(info);
        }
    };

    class ResolveRequest final : public hx::asys::libuv::BaseRequest
    {
        std::unique_ptr<hx::strbuf> pathBuffer;

    public:
        uv_getaddrinfo_t uv;

        ResolveRequest(Dynamic _cbSuccess, Dynamic _cbFailure, std::unique_ptr<hx::strbuf> _pathBuffer)
            : BaseRequest(_cbSuccess, _cbFailure)
            , pathBuffer(std::move(_pathBuffer))
        {
            uv.data = this;
        }

        static void onCallback(uv_getaddrinfo_t* request, int status, addrinfo* addr)
        {
            auto gcZone      = hx::AutoGCZone();
            auto spRequest   = std::unique_ptr<ResolveRequest>(static_cast<ResolveRequest*>(request->data));
            auto addrCleaner = AddrInfoCleaner(addr);

            if (status < 0)
            {
                Dynamic(spRequest->cbFailure.rooted)(hx::asys::libuv::uv_err_to_enum(status));
            }
            else
            {
                auto ips  = new Array_obj<hx::EnumBase>(0, 0);
                auto info = addr;

                do
                {
                    switch (info->ai_addr->sa_family)
                    {
                    case AF_INET: {
                        ips->Add(hx::asys::libuv::net::ip_from_sockaddr(reinterpret_cast<sockaddr_in*>(info->ai_addr)));
                        break;
                    }

                    case AF_INET6: {
                        ips->Add(hx::asys::libuv::net::ip_from_sockaddr(reinterpret_cast<sockaddr_in6*>(info->ai_addr)));
                        break;
                    }

                    // TODO : What should we do if its another type?
                    }

                    info = info->ai_next;
                } while (nullptr != info);

                Dynamic(spRequest->cbSuccess.rooted)(ips);
            }
        }
    };

    class ResolveWork final : public hx::asys::libuv::WorkRequest
    {
        std::unique_ptr<hx::strbuf> hostBuffer;
        const char* host;

    public:
        ResolveWork(Dynamic _cbSuccess, Dynamic _cbFailure, std::unique_ptr<hx::strbuf> _hostBuffer, const char* _host)
            : WorkRequest(_cbSuccess, _cbFailure)
            , hostBuffer(std::move(_hostBuffer))
            , host(_host) {}

        void run(uv_loop_t* loop) override
        {
            auto hints = addrinfo();
            hints.ai_family   = AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_protocol = IPPROTO_TCP;
            hints.ai_flags    = 0;

            auto request = std::make_unique<ResolveRequest>(cbSuccess.rooted, cbFailure.rooted, std::move(hostBuffer));
            auto result  = uv_getaddrinfo(loop, &request->uv, ResolveRequest::onCallback, host, nullptr, &hints);
            if (result < 0)
            {
                Dynamic(cbFailure.rooted)(hx::asys::libuv::uv_err_to_enum(result));
            }
            else
            {
                request.release();
            }
        }
    };

    auto libuvCtx   = hx::asys::libuv::context(ctx);
    auto hostBuffer = std::make_unique<hx::strbuf>();
    auto hostString = host.utf8_str(hostBuffer.get());

    libuvCtx->ctx->enqueue(std::move(std::make_unique<ResolveWork>(cbSuccess, cbFailure, std::move(hostBuffer), hostString)));
}

void hx::asys::net::dns::reverse(Context ctx, const Ipv4Address ip, Dynamic cbSuccess, Dynamic cbFailure)
{
    class ReverseWork final : public hx::asys::libuv::WorkRequest
    {
        sockaddr_in addr;

    public:
        ReverseWork(Dynamic _cbSuccess, Dynamic _cbFailure, sockaddr_in _addr) : WorkRequest(_cbSuccess, _cbFailure), addr(_addr)
        {
            //
        }

        void run(uv_loop_t* loop) override
        {
            auto request = std::make_unique<ReverseRequest>(cbSuccess.rooted, cbFailure.rooted);
            auto result = uv_getnameinfo(loop, &request->uv, ReverseRequest::callback, reinterpret_cast<sockaddr*>(&addr), 0);
            if (result < 0)
            {
                Dynamic(cbFailure.rooted)(hx::asys::libuv::uv_err_to_enum(result));
            }
            else
            {
                request.release();
            }
        }
    };

    auto libuvCtx = hx::asys::libuv::context(ctx);
    auto addr     = hx::asys::libuv::net::sockaddr_from_int(ip);

    libuvCtx->ctx->enqueue(std::move(std::make_unique<ReverseWork>(cbSuccess, cbFailure, addr)));
}

void hx::asys::net::dns::reverse(Context ctx, const Ipv6Address ip, Dynamic cbSuccess, Dynamic cbFailure)
{
    class ReverseWork final : public hx::asys::libuv::WorkRequest
    {
        sockaddr_in6 addr;

    public:
        ReverseWork(Dynamic _cbSuccess, Dynamic _cbFailure, sockaddr_in6 _addr) : WorkRequest(_cbSuccess, _cbFailure), addr(_addr)
        {
            //
        }

        void run(uv_loop_t* loop) override
        {
            auto request = std::make_unique<ReverseRequest>(cbSuccess.rooted, cbFailure.rooted);
            auto result = uv_getnameinfo(loop, &request->uv, ReverseRequest::callback, reinterpret_cast<sockaddr*>(&addr), 0);
            if (result < 0)
            {
                Dynamic(cbFailure.rooted)(hx::asys::libuv::uv_err_to_enum(result));
            }
            else
            {
                request.release();
            }
        }
    };

    auto libuvCtx = hx::asys::libuv::context(ctx);
    auto addr     = hx::asys::libuv::net::sockaddr_from_data(ip);

    libuvCtx->ctx->enqueue(std::move(std::make_unique<ReverseWork>(cbSuccess, cbFailure, addr)));
}