#pragma once

#include <hxcpp.h>
#include "../LibuvUtils.h"

namespace hx::asys::libuv::filesystem
{
    struct FsRequest : hx::asys::libuv::BaseRequest
    {
        hx::strbuf buffer;

    public:
        uv_fs_t uv;

        FsRequest(std::unique_ptr<RootedCallbacks> _callbacks) : BaseRequest(std::move(_callbacks))
        {
            uv.data = this;
        }

        virtual ~FsRequest()
        {
            uv_fs_req_cleanup(&uv);
        }

        static void callback(uv_fs_t* request)
        {
            auto gcZone    = hx::AutoGCZone();
            auto spRequest = std::unique_ptr<FsRequest>(static_cast<FsRequest*>(request->data));

            if (spRequest->uv.result < 0)
            {
                spRequest->callbacks->fail(hx::asys::libuv::uv_err_to_enum(spRequest->uv.result));
            }
            else
            {
                spRequest->callbacks->succeed(spRequest->uv.result);
            }
        }
    };
}