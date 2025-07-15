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
		class Ctx
		{
			static void onCloseCallback(uv_handle_t* handle);

		public:
			/// <summary>
			/// 
			/// </summary>
			uv_tcp_t tcp;

			/// <summary>
			/// 
			/// </summary>
			uv_shutdown_t shutdown;

			/// <summary>
			/// 
			/// </summary>
			std::unique_ptr<RootedCallbacks> callbacks;

			/// <summary>
			/// 
			/// </summary>
			hx::asys::libuv::stream::StreamReader_obj::Ctx stream;

			/// <summary>
			/// 
			/// </summary>
			std::atomic_bool closed;

			/// <summary>
			/// 
			/// </summary>
			int status;

			Ctx();

			void close();
		};

		Ctx* ctx;

		LibuvTcpSocket(Ctx* _ctx, ::hx::Anon _localAddress, ::hx::Anon _remoteAddress);

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
