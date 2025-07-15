#pragma once

#include <hxcpp.h>
#include <deque>
#include <array>
#include <optional>
#include <map>
#include "../LibuvUtils.h"
#include "../stream/StreamReader.h"
#include "../stream/StreamWriter.h"

namespace hx::asys::libuv::system
{
	class LibuvCurrentProcess final : public hx::asys::system::CurrentProcess_obj
	{
	public:
		struct Ctx
		{
			hx::asys::libuv::stream::StreamReader_obj::Ctx reader;

			Ctx(uv_stream_t* _stdin);
		};

		Ctx* ctx;

		LibuvCurrentProcess(Ctx* _ctx, uv_stream_t* _stdout, uv_stream_t* _stderr);

		Pid pid() override;

		//void sendSignal(hx::EnumBase signal, Dynamic cbSuccess, Dynamic cbFailure) override;

		//void setSignalAction(hx::EnumBase signal, hx::EnumBase action);

		void __Mark(hx::MarkContext* __inCtx) override;
#ifdef HXCPP_VISIT_ALLOCS
		void __Visit(hx::VisitContext* __inCtx) override;
#endif
	};
}