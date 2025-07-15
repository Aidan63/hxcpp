#include <hxcpp.h>
#include "LibuvTcpSocket.h"
#include "NetUtils.h"
#include "../LibuvUtils.h"
#include <optional>

namespace
{
	struct ConnectRequest final : public hx::asys::libuv::BaseRequest
	{
		uv_connect_t connect;
		std::unique_ptr<hx::asys::libuv::net::LibuvTcpSocket::Ctx> ctx;

		ConnectRequest(std::unique_ptr<hx::asys::libuv::RootedCallbacks> _callbacks, std::unique_ptr<hx::asys::libuv::net::LibuvTcpSocket::Ctx> _ctx)
			: BaseRequest(std::move(_callbacks))
			, ctx(std::move(_ctx))
		{
			connect.data = this;
		}

		static void onCallback(uv_connect_t* connect, const int status)
		{
			auto gcZone  = hx::AutoGCZone();
			auto request = std::unique_ptr<ConnectRequest>(static_cast<ConnectRequest*>(connect->data));

			if (status < 0)
			{
				request->ctx->status    = status;
				request->ctx->callbacks = std::move(request->callbacks);
				request->ctx->close();

				return;
			}

			::hx::Anon local;
			if ((request->ctx->status = hx::asys::libuv::net::getLocalAddress(&request->ctx->tcp, local)) < 0)
			{
				request->ctx->callbacks = std::move(request->callbacks);
				request->ctx->close();

				return;
			}

			::hx::Anon remote;
			if ((request->ctx->status = hx::asys::libuv::net::getRemoteAddress(&request->ctx->tcp, remote)) < 0)
			{
				request->ctx->callbacks = std::move(request->callbacks);
				request->ctx->close();

				return;
			}

			request->callbacks->succeed(new hx::asys::libuv::net::LibuvTcpSocket(request->ctx.release(), local, remote));
		}
	};

	class TcpConnectWork : public hx::asys::libuv::CallbackWorkRequest
	{
		const std::optional<int> keepAlive;
		const std::optional<int> sendBuffer;
		const std::optional<int> recvBuffer;

		virtual const sockaddr* address() = 0;

	public:
		TcpConnectWork(Dynamic _cbSuccess, Dynamic _cbFailure, std::optional<int> _keepAlive, std::optional<int> _sendBuffer, std::optional<int> _recvBuffer)
			: CallbackWorkRequest(_cbSuccess, _cbFailure)
			, keepAlive(_keepAlive)
			, sendBuffer(_sendBuffer)
			, recvBuffer(_recvBuffer) {
		}

		void run(uv_loop_t* loop) override final
		{
			auto ctx    = std::make_unique<hx::asys::libuv::net::LibuvTcpSocket::Ctx>();
			auto gcZone = hx::AutoGCZone();

			if ((ctx->status = uv_tcp_init(loop, &ctx->tcp)) < 0)
			{
				callbacks->fail(hx::asys::libuv::uv_err_to_enum(ctx->status));

				return;
			}

			if ((ctx->status = uv_tcp_keepalive(&ctx->tcp, keepAlive.has_value(), keepAlive.value_or(0))) < 0)
			{
				ctx->callbacks = std::move(callbacks);
				ctx->close();

				return;
			}

			if (sendBuffer.has_value())
			{
				auto value = sendBuffer.value();
				if ((ctx->status = uv_send_buffer_size(reinterpret_cast<uv_handle_t*>(&ctx->tcp), &value)) < 0)
				{
					ctx->callbacks = std::move(callbacks);
					ctx->close();

					return;
				}
			}

			if (recvBuffer.has_value())
			{
				auto value = recvBuffer.value();
				if ((ctx->status = uv_recv_buffer_size(reinterpret_cast<uv_handle_t*>(&ctx->tcp), &value)) < 0)
				{
					ctx->callbacks = std::move(callbacks);
					ctx->close();

					return;
				}
			}

			auto request = std::make_unique<ConnectRequest>(std::move(callbacks), std::move(ctx));
			if ((request->ctx->status = uv_tcp_connect(&request->connect, &request->ctx->tcp, address(), ConnectRequest::onCallback)) < 0)
			{
				request->ctx->callbacks = std::move(request->callbacks);
				request->ctx->close();
			}
			else
			{
				request.release();
			}
		}
	};

	void onAlloc(uv_handle_t* handle, const size_t suggested, uv_buf_t* buffer)
	{
		auto  ctx     = static_cast<hx::asys::libuv::net::LibuvTcpSocket::Ctx*>(handle->data);
		auto& staging = ctx->stream.staging.emplace_back(suggested);

		buffer->base = staging.data();
		buffer->len  = staging.size();
	}

	void onRead(uv_stream_t* stream, const ssize_t len, const uv_buf_t* read)
	{
		auto gc  = hx::AutoGCZone();
		auto ctx = static_cast<hx::asys::libuv::net::LibuvTcpSocket::Ctx*>(stream->data);

		if (len <= 0)
		{
			ctx->stream.reject(len);

			return;
		}

		ctx->stream.buffer.insert(ctx->stream.buffer.end(), read->base, read->base + len);
		ctx->stream.consume();
	}
}

hx::asys::libuv::net::LibuvTcpSocket::LibuvTcpSocket(Ctx* _ctx, ::hx::Anon _localAddress, ::hx::Anon _remoteAddress)
	: ctx(_ctx)
{
	HX_OBJ_WB_NEW_MARKED_OBJECT(this);

	reader        = hx::asys::Readable(new hx::asys::libuv::stream::StreamReader_obj(ctx->stream));
	writer        = hx::asys::Writable(new hx::asys::libuv::stream::StreamWriter_obj(reinterpret_cast<uv_stream_t*>(&ctx->tcp)));
	localAddress  = _localAddress;
	remoteAddress = _remoteAddress;
}

//void hx::asys::libuv::net::LibuvTcpSocket::getKeepAlive(Dynamic cbSuccess, Dynamic cbFailure)
//{
//	cbSuccess(ctx->keepAlive > 0);
//}

//void hx::asys::libuv::net::LibuvTcpSocket::getSendBufferSize(Dynamic cbSuccess, Dynamic cbFailure)
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
//void hx::asys::libuv::net::LibuvTcpSocket::getRecvBufferSize(Dynamic cbSuccess, Dynamic cbFailure)
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
//void hx::asys::libuv::net::LibuvTcpSocket::setKeepAlive(bool keepAlive, Dynamic cbSuccess, Dynamic cbFailure)
//{
//	auto result = uv_tcp_keepalive(&ctx->tcp, keepAlive, keepAlive ? KEEP_ALIVE_VALUE : 0);
//	if (result < 0)
//	{
//		cbFailure(hx::asys::libuv::uv_err_to_enum(result));
//	}
//	else
//	{
//		ctx->keepAlive = keepAlive ? KEEP_ALIVE_VALUE : 0;
//
//		cbSuccess();
//	}
//}
//
//void hx::asys::libuv::net::LibuvTcpSocket::setSendBufferSize(int size, Dynamic cbSuccess, Dynamic cbFailure)
//{
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
//void hx::asys::libuv::net::LibuvTcpSocket::setRecvBufferSize(int size, Dynamic cbSuccess, Dynamic cbFailure)
//{
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
void hx::asys::libuv::net::LibuvTcpSocket::close(Dynamic cbSuccess, Dynamic cbFailure)
{
	class CloseWork final : public hx::asys::libuv::CallbackWorkRequest
	{
		Ctx* ctx;

	public:
		CloseWork(Dynamic _cbSuccess, Dynamic _cbFailure, Ctx* _ctx)
			: CallbackWorkRequest(_cbSuccess, _cbFailure)
			, ctx(_ctx) {}

		void run(uv_loop_t* loop) override
		{
			auto gcZone = hx::AutoGCZone();

			ctx->callbacks = std::move(callbacks);

			auto result  = uv_shutdown(&ctx->shutdown, reinterpret_cast<uv_stream_t*>(&ctx->tcp), onShutdownCallback);
			if (result < 0)
			{
				ctx->close();
			}
		}

		static void onShutdownCallback(uv_shutdown_t* shutdown, int status)
		{
			auto ctx = static_cast<Ctx*>(shutdown->data);

			ctx->close();
		}
	};

	auto expected = false;
	if (false == ctx->closed.compare_exchange_strong(expected, true))
	{
		cbSuccess();

		return;
	}

	auto libuv = static_cast<LibuvAsysContext_obj::Ctx*>(ctx->tcp.loop->data);

	libuv->emplace<CloseWork>(cbSuccess.mPtr, cbFailure.mPtr, ctx);
}

void hx::asys::libuv::net::LibuvTcpSocket::__Mark(hx::MarkContext* __inCtx)
{
	HX_MARK_MEMBER(localAddress);
	HX_MARK_MEMBER(remoteAddress);
	HX_MARK_MEMBER(reader);
	HX_MARK_MEMBER(writer);
}

#ifdef HXCPP_VISIT_ALLOCS
void hx::asys::libuv::net::LibuvTcpSocket::__Visit(hx::VisitContext* __inCtx)
{
	HX_VISIT_MEMBER(localAddress);
	HX_VISIT_MEMBER(remoteAddress);
	HX_VISIT_MEMBER(reader);
	HX_VISIT_MEMBER(writer);
}
#endif

void hx::asys::net::TcpSocket_obj::connect_ipv4(Context ctx, const String host, int port, Dynamic _options, Dynamic cbSuccess, Dynamic cbFailure)
{
	class Ipv4TcpConnectWork final : public TcpConnectWork
	{
		sockaddr* address() override
		{
			return reinterpret_cast<sockaddr*>(addr.get());
		}

	public:
		std::unique_ptr<sockaddr_in> addr;

		Ipv4TcpConnectWork(Dynamic _cbSuccess, Dynamic _cbFailure, std::optional<int> _keepAlive, std::optional<int> _sendBuffer, std::optional<int> _recvBuffer, std::unique_ptr<sockaddr_in> _addr)
			: TcpConnectWork(_cbSuccess, _cbFailure, _keepAlive, _sendBuffer, _recvBuffer)
			, addr(std::move(_addr))
		{
			//
		}
	};

	auto keepAlive = std::optional<int>();
	auto sendBuffer = std::optional<int>();
	auto recvBuffer = std::optional<int>();

	if (hx::IsNotNull(_options))
	{
		hx::Val found;

		found = _options->__Field(HX_CSTRING("keepAlive"), hx::PropertyAccess::paccDynamic);
		if (found.isBool())
		{
			keepAlive.emplace(found.asInt() ? hx::asys::libuv::net::KEEP_ALIVE_VALUE : 0);
		}

		found = _options->__Field(HX_CSTRING("sendBuffer"), hx::PropertyAccess::paccDynamic);
		if (found.isInt())
		{
			sendBuffer.emplace(found.asInt());
		}

		found = _options->__Field(HX_CSTRING("recvBuffer"), hx::PropertyAccess::paccDynamic);
		if (found.isInt())
		{
			recvBuffer.emplace(found.asInt());
		}
	}

	hx::strbuf buffer;

	auto libuv  = hx::asys::libuv::context(ctx);
	auto addr   = std::make_unique<sockaddr_in>();
	auto result = uv_ip4_addr(host.utf8_str(&buffer), port, addr.get());

	if (result < 0)
	{
		cbFailure(hx::asys::libuv::uv_err_to_enum(result));

		return;
	}
	
	libuv->ctx->emplace<Ipv4TcpConnectWork>(cbSuccess, cbFailure, keepAlive, sendBuffer, recvBuffer, std::move(addr));
}

void hx::asys::net::TcpSocket_obj::connect_ipv6(Context ctx, const String host, int port, Dynamic options, Dynamic cbSuccess, Dynamic cbFailure)
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
		startConnection(hx::asys::libuv::context(ctx), reinterpret_cast<sockaddr*>(&address), options, cbSuccess, cbFailure);
	}*/
}

hx::asys::libuv::net::LibuvTcpSocket::Ctx::Ctx()
	: stream(reinterpret_cast<uv_stream_t*>(&tcp), onAlloc, onRead)
{
	tcp.data = shutdown.data = this;
}

void hx::asys::libuv::net::LibuvTcpSocket::Ctx::onCloseCallback(uv_handle_t* handle)
{
	auto gcZone = hx::AutoGCZone();
	auto ctx    = std::unique_ptr<Ctx>(static_cast<Ctx*>(handle->data));

	if (0 == ctx->status)
	{
		ctx->callbacks->succeed();
	}
	else
	{
		ctx->callbacks->fail(hx::asys::libuv::uv_err_to_enum(ctx->status));
	}
}

void hx::asys::libuv::net::LibuvTcpSocket::Ctx::close()
{
	uv_close(reinterpret_cast<uv_handle_t*>(&tcp), onCloseCallback);
}
