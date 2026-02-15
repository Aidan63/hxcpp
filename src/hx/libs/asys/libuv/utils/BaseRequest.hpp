#pragma once

#ifndef HXCPP_H
#include <hxcpp.h>
#endif

#include <memory>
#include "RootedCallbacks.hpp"

namespace hx::asys::libuv
{
	struct BaseRequest
	{
		std::unique_ptr<RootedCallbacks> callbacks;

		BaseRequest(std::unique_ptr<RootedCallbacks> _callbacks) : callbacks(std::move(_callbacks)) {}

		virtual ~BaseRequest() = default;
	};
}