#pragma once

#ifndef HXCPP_H
#include<hxcpp.h>
#endif

#include <uv.h>
#include "RootedCallbacks.hpp"

namespace hx::asys::libuv
{
	struct WorkRequest
	{
		virtual void run(uv_loop_t* _loop) = 0;

		virtual ~WorkRequest() = default;
	};
}