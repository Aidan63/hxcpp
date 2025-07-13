#include <hxcpp.h>
#include "LibuvTcpServer.h"
#include "LibuvTcpSocket.h"
#include "NetUtils.h"
#include "../LibuvUtils.h"

namespace
{
	void onConnection(uv_stream_t* stream, int status)
	{
		auto server = static_cast<hx::asys::libuv::net::LibuvTcpServer::Ctx*>(stream->data);

		if (status < 0)
		{
			// uv_close(reinterpret_cast<uv_handle_t*>(stream), hx::asys::libuv::net::LibuvTcpServer::Ctx::failure);

			return;
		}
		else
		{
			auto request = server->connections.tryDequeue();
			if (nullptr != request)
			{
				auto tcp    = std::make_unique<uv_tcp_t>();
				auto result = 0;

				if ((result = uv_tcp_init(server->tcp->loop, tcp.get())) < 0)
				{
					return;
				}

				if ((result = uv_accept(reinterpret_cast<uv_stream_t*>(server->tcp.get()), reinterpret_cast<uv_stream_t*>(tcp.get()))) < 0)
				{
					return;
				}

				auto gcZone = hx::AutoGCZone();

				::hx::Anon local;
				if ((result = hx::asys::libuv::net::getLocalAddress(tcp.get(), local)) < 0)
				{
					return;
				}

				::hx::Anon remote;
				if ((result = hx::asys::libuv::net::getRemoteAddress(tcp.get(), remote)) < 0)
				{
					return;
				}

				/*if ((result = uv_tcp_keepalive(&ctx->tcp, ctx->keepAlive > 0, ctx->keepAlive)) < 0)
				{
					Dynamic(request->cbFailure.rooted)(hx::asys::libuv::uv_err_to_enum(ctx->status));

					return;
				}*/

				Dynamic(request->cbSuccess.rooted)(hx::asys::net::TcpSocket(new hx::asys::libuv::net::LibuvTcpSocket(std::move(tcp), local, remote)));
			}
		}
	}

	/*void onOpen(hx::asys::libuv::LibuvAsysContext ctx, sockaddr* const address, Dynamic options, Dynamic cbSuccess, Dynamic cbFailure)
	{
		auto backlog        = SOMAXCONN;
		auto sendBufferSize = 0;
		auto recvBufferSize = 0;
		auto keepAlive      = hx::asys::libuv::net::KEEP_ALIVE_VALUE;

		if (hx::IsNotNull(options))
		{
			auto backlogSizeVal = options->__Field(HX_CSTRING("backlog"), hx::PropertyAccess::paccDynamic);
			if (backlogSizeVal.isInt())
			{
				backlog = backlogSizeVal.asInt();
			}

			auto keepAliveVal = options->__Field(HX_CSTRING("keepAlive"), hx::PropertyAccess::paccDynamic);
			if (keepAliveVal.isBool())
			{
				keepAlive = keepAliveVal.asInt() ? hx::asys::libuv::net::KEEP_ALIVE_VALUE : 0;
			}
		}

		auto server = std::make_unique<hx::asys::libuv::net::LibuvTcpServer::Ctx>(cbSuccess, cbFailure, keepAlive);
		auto result = 0;

		if ((result = uv_tcp_init(ctx->uvLoop, &server->tcp)) < 0)
		{
			cbFailure(hx::asys::libuv::uv_err_to_enum(result));

			return;
		}

		if ((server->status = uv_tcp_bind(&server->tcp, address, 0)) < 0)
		{
			uv_close(reinterpret_cast<uv_handle_t*>(&server.release()->tcp), hx::asys::libuv::net::LibuvTcpServer::Ctx::failure);

			return;
		}

		if (hx::IsNotNull(options))
		{
			auto sendBufferVal = options->__Field(HX_CSTRING("sendBuffer"), hx::PropertyAccess::paccDynamic);
			if (sendBufferVal.isInt())
			{
				auto size = sendBufferVal.asInt();

				if (size > 0)
				{
					if ((server->status = uv_send_buffer_size(reinterpret_cast<uv_handle_t*>(&server->tcp), &size)) < 0)
					{
						uv_close(reinterpret_cast<uv_handle_t*>(&server.release()->tcp), hx::asys::libuv::net::LibuvTcpServer::Ctx::failure);

						return;
					}
				}
				else
				{
					server->status = UV_EINVAL;

					uv_close(reinterpret_cast<uv_handle_t*>(&server.release()->tcp), hx::asys::libuv::net::LibuvTcpServer::Ctx::failure);

					return;
				}
			}

			auto recvBufferVal = options->__Field(HX_CSTRING("receiveBuffer"), hx::PropertyAccess::paccDynamic);
			if (recvBufferVal.isInt())
			{
				auto size = recvBufferVal.asInt();

				if (size > 0)
				{
					if ((server->status = uv_recv_buffer_size(reinterpret_cast<uv_handle_t*>(&server->tcp), &size)) < 0)
					{
						uv_close(reinterpret_cast<uv_handle_t*>(&server.release()->tcp), hx::asys::libuv::net::LibuvTcpServer::Ctx::failure);

						return;
					}
				}
				else
				{
					server->status = UV_EINVAL;

					uv_close(reinterpret_cast<uv_handle_t*>(&server.release()->tcp), hx::asys::libuv::net::LibuvTcpServer::Ctx::failure);

					return;
				}
			}
		}

		if ((server->status = uv_listen(reinterpret_cast<uv_stream_t*>(&server->tcp), backlog, onConnection)) < 0)
		{
			uv_close(reinterpret_cast<uv_handle_t*>(&server.release()->tcp), hx::asys::libuv::net::LibuvTcpServer::Ctx::failure);

			return;
		}
		else
		{
			cbSuccess(hx::asys::net::TcpServer(new hx::asys::libuv::net::LibuvTcpServer(server.release())));
		}
	}*/
}

hx::asys::libuv::net::LibuvTcpServer::ConnectionQueue::ConnectionQueue() : queue(0)
{

}

void hx::asys::libuv::net::LibuvTcpServer::ConnectionQueue::clear()
{
	queue.clear();
}

void hx::asys::libuv::net::LibuvTcpServer::ConnectionQueue::enqueue(Dynamic cbSuccess, Dynamic cbFailure)
{
	queue.push_back(std::make_unique<hx::asys::libuv::BaseRequest>(cbSuccess, cbFailure));
}

std::unique_ptr<hx::asys::libuv::BaseRequest> hx::asys::libuv::net::LibuvTcpServer::ConnectionQueue::tryDequeue()
{
	if (queue.empty())
	{
		return nullptr;
	}

	auto root = std::unique_ptr<hx::asys::libuv::BaseRequest>{ std::move(queue.front()) };

	queue.pop_front();

	return root;
}

hx::asys::libuv::net::LibuvTcpServer::Ctx::Ctx(std::unique_ptr<uv_tcp_t> _tcp) : tcp(std::move(_tcp))
{
	tcp->data = this;
}

hx::asys::libuv::net::LibuvTcpServer::LibuvTcpServer(hx::asys::libuv::net::LibuvTcpServer::Ctx* ctx, ::hx::Anon _localAddress) : ctx(ctx)
{
	HX_OBJ_WB_NEW_MARKED_OBJECT(this);

	localAddress = _localAddress;
}

void hx::asys::libuv::net::LibuvTcpServer::accept(Dynamic cbSuccess, Dynamic cbFailure)
{
	class AcceptWork final : public hx::asys::libuv::WorkRequest
	{
		Ctx* ctx;

	public:
		AcceptWork(Dynamic _cbSuccess, Dynamic _cbFailure, Ctx* _ctx) : WorkRequest(_cbSuccess, _cbFailure), ctx(_ctx) { }

		void run(uv_loop_t* loop) override
		{
			ctx->connections.enqueue(cbSuccess.rooted, cbFailure.rooted);
		}
	};

	auto libuv = static_cast<LibuvAsysContext_obj::Ctx*>(ctx->tcp->loop->data);

	libuv->emplace<AcceptWork>(cbSuccess, cbFailure, ctx);
}

//void hx::asys::libuv::net::LibuvTcpServer::getKeepAlive(Dynamic cbSuccess, Dynamic cbFailure)
//{
//	cbSuccess(ctx->keepAlive > 0);
//}
//
//void hx::asys::libuv::net::LibuvTcpServer::getSendBufferSize(Dynamic cbSuccess, Dynamic cbFailure)
//{
//	auto size   = 0;
//	auto result = uv_send_buffer_size(reinterpret_cast<uv_handle_t*>(&ctx->tcp), &size);
//
//	if (result < 0)
//	{
//		cbFailure(hx::asys::libuv::uv_err_to_enum(result));
//	}
//	else
//	{
//		cbSuccess(size);
//	}
//}
//
//void hx::asys::libuv::net::LibuvTcpServer::getRecvBufferSize(Dynamic cbSuccess, Dynamic cbFailure)
//{
//	auto size   = 0;
//	auto result = uv_recv_buffer_size(reinterpret_cast<uv_handle_t*>(&ctx->tcp), &size);
//
//	if (result < 0)
//	{
//		cbFailure(hx::asys::libuv::uv_err_to_enum(result));
//	}
//	else
//	{
//		cbSuccess(size);
//	}
//}
//
//void hx::asys::libuv::net::LibuvTcpServer::setKeepAlive(bool keepAlive, Dynamic cbSuccess, Dynamic cbFailure)
//{
//	ctx->keepAlive = keepAlive ? KEEP_ALIVE_VALUE : 0;
//
//	cbSuccess();
//}
//
//void hx::asys::libuv::net::LibuvTcpServer::setSendBufferSize(int size, Dynamic cbSuccess, Dynamic cbFailure)
//{
//	if (size > 0)
//	{
//		auto result = uv_send_buffer_size(reinterpret_cast<uv_handle_t*>(&ctx->tcp), &size);
//
//		if (result < 0)
//		{
//			cbFailure(hx::asys::libuv::uv_err_to_enum(result));
//		}
//		else
//		{
//			cbSuccess(size);
//		}
//	}
//	else
//	{
//		cbFailure(hx::asys::libuv::uv_err_to_enum(UV_EINVAL));
//	}
//}
//
//void hx::asys::libuv::net::LibuvTcpServer::setRecvBufferSize(int size, Dynamic cbSuccess, Dynamic cbFailure)
//{
//	if (size > 0)
//	{
//		auto result = uv_recv_buffer_size(reinterpret_cast<uv_handle_t*>(&ctx->tcp), &size);
//
//		if (result < 0)
//		{
//			cbFailure(hx::asys::libuv::uv_err_to_enum(result));
//		}
//		else
//		{
//			cbSuccess(size);
//		}
//	}
//	else
//	{
//		cbFailure(hx::asys::libuv::uv_err_to_enum(UV_EINVAL));
//	}
//}

void hx::asys::libuv::net::LibuvTcpServer::__Mark(hx::MarkContext* __inCtx)
{
	HX_MARK_MEMBER(localAddress);
}

#ifdef HXCPP_VISIT_ALLOCS
void hx::asys::libuv::net::LibuvTcpServer::__Visit(hx::VisitContext* __inCtx)
{
	HX_VISIT_MEMBER(localAddress);
}
#endif

void hx::asys::net::TcpServer_obj::open_ipv4(Context ctx, const String host, int port, Dynamic options, Dynamic cbSuccess, Dynamic cbFailure)
{
	class Ipv4OpenWork final : public hx::asys::libuv::WorkRequest
	{
	public:
		sockaddr_in addr;

		Ipv4OpenWork(Dynamic _cbSuccess, Dynamic _cbFailure) : WorkRequest(_cbSuccess, _cbFailure) {}

		void run(uv_loop_t* loop) override
		{
			auto tcp    = std::make_unique<uv_tcp_t>();
			auto result = 0;

			if ((result = uv_tcp_init(loop, tcp.get())) < 0)
			{
				return;
			}

			if ((result = uv_tcp_bind(tcp.get(), reinterpret_cast<sockaddr*>(&addr), 0)) < 0)
			{
				return;
			}

			if ((result = uv_listen(reinterpret_cast<uv_stream_t*>(tcp.get()), SOMAXCONN, onConnection)) < 0)
			{
				return;
			}

			auto gcZone = hx::AutoGCZone();

			::hx::Anon local;
			if ((result = hx::asys::libuv::net::getLocalAddress(tcp.get(), local)) < 0)
			{
				return;
			}

			Dynamic(cbSuccess.rooted)(new hx::asys::libuv::net::LibuvTcpServer(new hx::asys::libuv::net::LibuvTcpServer::Ctx(std::move(tcp)), local));
		}
	};

	hx::strbuf buffer;

	auto libuv  = hx::asys::libuv::context(ctx);
	auto addr   = std::make_unique<sockaddr_in>();
	auto result = uv_ip4_addr(host.utf8_str(&buffer), port, addr.get());

	if (result < 0)
	{
		cbFailure(hx::asys::libuv::uv_err_to_enum(result));

		return;
	}

	libuv->ctx->emplace<Ipv4OpenWork>(cbSuccess, cbFailure);
}

void hx::asys::net::TcpServer_obj::open_ipv6(Context ctx, const String host, int port, Dynamic options, Dynamic cbSuccess, Dynamic cbFailure)
{
	/*hx::strbuf buffer;

	auto address = sockaddr_in6();
	auto result  = uv_ip6_addr(host.utf8_str(&buffer), port, &address);

	if (result < 0)
	{
		cbFailure(hx::asys::libuv::uv_err_to_enum(result));
	}
	else
	{
		onOpen(hx::asys::libuv::context(ctx), reinterpret_cast<sockaddr*>(&address), options, cbSuccess, cbFailure);
	}*/
}