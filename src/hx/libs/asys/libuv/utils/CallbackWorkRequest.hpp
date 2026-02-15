#pragma once

#ifndef HXCPP_H
#include <hxcpp.h>
#endif

#include <memory>
#include "RootedCallbacks.hpp"
#include "WorkRequest.hpp"

namespace hx::asys::libuv
{
	class CallbackWorkRequest : public WorkRequest
	{
	protected:
		std::unique_ptr<RootedCallbacks> callbacks;

	public:
		CallbackWorkRequest(Dynamic _cbSuccess, Dynamic _cbFailure)
			: callbacks(std::make_unique<RootedCallbacks>(_cbSuccess.GetPtr(), _cbFailure.GetPtr())) {}

		virtual ~CallbackWorkRequest() = default;
	};
}