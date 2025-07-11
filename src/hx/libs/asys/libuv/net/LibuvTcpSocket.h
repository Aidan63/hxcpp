#pragma once

#include <hxcpp.h>
#include <atomic>
#include "../stream/StreamReader.h"
#include "../stream/StreamWriter.h"

namespace hx::asys::libuv::net
{
	const int KEEP_ALIVE_VALUE = 5;

	class LibuvTcpSocket final : public hx::asys::net::TcpSocket_obj
	{
	public:
		struct Ctx
		{
			std::unique_ptr<uv_tcp_t> tcp;
			std::atomic_bool closed;
			hx::asys::libuv::stream::StreamReader_obj::Ctx stream;

			Ctx(std::unique_ptr<uv_tcp_t> _tcp, uv_alloc_cb _cbAlloc, uv_read_cb _cbRead);
		};

		Ctx* ctx;

		LibuvTcpSocket(std::unique_ptr<uv_tcp_t> _tcp, ::hx::Anon _localAddress, ::hx::Anon _remoteAddress);

		//void getKeepAlive(Dynamic cbSuccess, Dynamic cbFailure) override;
		//void getSendBufferSize(Dynamic cbSuccess, Dynamic cbFailure) override;
		//void getRecvBufferSize(Dynamic cbSuccess, Dynamic cbFailure) override;

		//void setKeepAlive(bool keepAlive, Dynamic cbSuccess, Dynamic cbFailure) override;
		//void setSendBufferSize(int size, Dynamic cbSuccess, Dynamic cbFailure) override;
		//void setRecvBufferSize(int size, Dynamic cbSuccess, Dynamic cbFailure) override;

		void close(Dynamic cbSuccess, Dynamic cbFailure) override;

		void __Mark(hx::MarkContext* __inCtx) override;
#if HXCPP_VISIT_ALLOCS
		void __Visit(hx::VisitContext* __inCtx) override;
#endif
	};
}
