#pragma once

#include <hxcpp.h>
#include <memory>
#include <deque>
#include "../LibuvUtils.h"

namespace hx::asys::libuv::net
{
	class LibuvTcpServer final : public hx::asys::net::TcpServer_obj
	{
	public:
		class ConnectionQueue
		{
			std::deque<std::unique_ptr<hx::asys::libuv::RootedCallbacks>> queue;

		public:
			int existing;

			ConnectionQueue();

			void clear();
			void enqueue(std::unique_ptr<hx::asys::libuv::RootedCallbacks> _callbacks);
			std::unique_ptr<hx::asys::libuv::RootedCallbacks> tryDequeue();
		};

		struct Ctx
		{
			std::unique_ptr<uv_tcp_t> tcp;
			ConnectionQueue connections;

			Ctx(std::unique_ptr<uv_tcp_t> _tcp);
		};

		Ctx* ctx;

		LibuvTcpServer(Ctx* _server, ::hx::Anon _localAddress);

		void accept(Dynamic cbSuccess, Dynamic cbFailure) override;
		//void close(Dynamic cbSuccess, Dynamic cbFailure) override;

		//void getKeepAlive(Dynamic cbSuccess, Dynamic cbFailure) override;
		//void getSendBufferSize(Dynamic cbSuccess, Dynamic cbFailure) override;
		//void getRecvBufferSize(Dynamic cbSuccess, Dynamic cbFailure) override;

		//void setKeepAlive(bool keepAlive, Dynamic cbSuccess, Dynamic cbFailure) override;
		//void setSendBufferSize(int size, Dynamic cbSuccess, Dynamic cbFailure) override;
		//void setRecvBufferSize(int size, Dynamic cbSuccess, Dynamic cbFailure) override;

		void __Mark(hx::MarkContext* __inCtx) override;
#ifdef HXCPP_VISIT_ALLOCS
		void __Visit(hx::VisitContext* __inCtx) override;
#endif
	};
}