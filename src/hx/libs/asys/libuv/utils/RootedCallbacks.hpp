#pragma once

#ifndef HXCPP_H
#include<hxcpp.h>
#endif

namespace hx::asys::libuv
{
	struct RootedCallbacks final
	{
		hx::RootedObject<hx::Object> cbSuccess;
		hx::RootedObject<hx::Object> cbFailure;

		RootedCallbacks(Dynamic _cbSuccess, Dynamic _cbFailure)
            : cbSuccess(_cbSuccess.GetPtr()), cbFailure(_cbFailure.GetPtr()) {}

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
}