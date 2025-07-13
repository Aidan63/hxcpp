#include <hxcpp.h>
#include "LibuvTcpSocket.h"
#include "NetUtils.h"
#include "../LibuvUtils.h"
#include <optional>

namespace
{
	class ConnectFailedRequest final : public hx::asys::libuv::BaseRequest
	{
		const int error;

		std::unique_ptr<uv_tcp_t> tcp;

		ConnectFailedRequest(Dynamic _cbSuccess, Dynamic _cbFailure, std::unique_ptr<uv_tcp_t> _tcp, const int _error)
			: BaseRequest(_cbSuccess, _cbFailure)
			, tcp(std::move(_tcp))
			, error(_error)
		{
			tcp->data = this;
		}

		static void onCallback(uv_handle_t* handle)
		{
			auto request = std::unique_ptr<ConnectFailedRequest>(static_cast<ConnectFailedRequest*>(handle->data));
			auto gcZone  = hx::AutoGCZone();

			Dynamic(request->cbFailure.rooted)(hx::asys::libuv::uv_err_to_enum(request->error));
		}

	public:
		static void cleanup(std::unique_ptr<uv_tcp_t> tcp, int error, Dynamic cbSuccess, Dynamic cbFailure)
		{
			auto request = new ConnectFailedRequest(cbSuccess, cbFailure, std::move(tcp), error);

			uv_close(reinterpret_cast<uv_handle_t*>(request->tcp.get()), onCallback);
		}
	};

	struct ConnectRequest final : public hx::asys::libuv::BaseRequest
	{
		uv_connect_t connect;
		std::unique_ptr<uv_tcp_t> tcp;

		ConnectRequest(Dynamic _cbSuccess, Dynamic _cbFailure, std::unique_ptr<uv_tcp_t> _tcp)
			: BaseRequest(_cbSuccess, _cbFailure)
			, tcp(std::move(_tcp))
		{
			connect.data = this;
		}

		static void onCallback(uv_connect_t* connect, const int status)
		{
			auto request = std::unique_ptr<ConnectRequest>(static_cast<ConnectRequest*>(connect->data));
			auto gcZone  = hx::AutoGCZone();

			if (status < 0)
			{
				ConnectFailedRequest::cleanup(std::move(request->tcp), status, request->cbSuccess.rooted, request->cbFailure.rooted);

				return;
			}

			int result;

			::hx::Anon local;
			if ((result = hx::asys::libuv::net::getLocalAddress(request->tcp.get(), local)) < 0)
			{
				ConnectFailedRequest::cleanup(std::move(request->tcp), result, request->cbSuccess.rooted, request->cbFailure.rooted);

				return;
			}

			::hx::Anon remote;
			if ((result = hx::asys::libuv::net::getRemoteAddress(request->tcp.get(), remote)) < 0)
			{
				ConnectFailedRequest::cleanup(std::move(request->tcp), result, request->cbSuccess.rooted, request->cbFailure.rooted);

				return;
			}

			Dynamic(request->cbSuccess.rooted)(new hx::asys::libuv::net::LibuvTcpSocket(std::move(request->tcp), local, remote));
		}
	};

	class TcpConnectWork : public hx::asys::libuv::WorkRequest
	{
		const std::optional<int> keepAlive;
		const std::optional<int> sendBuffer;
		const std::optional<int> recvBuffer;

		virtual const sockaddr* address() = 0;

	public:
		TcpConnectWork(Dynamic _cbSuccess, Dynamic _cbFailure, std::optional<int> _keepAlive, std::optional<int> _sendBuffer, std::optional<int> _recvBuffer)
			: WorkRequest(_cbSuccess, _cbFailure)
			, keepAlive(_keepAlive)
			, sendBuffer(_sendBuffer)
			, recvBuffer(_recvBuffer) {
		}

		void run(uv_loop_t* loop) override final
		{
			auto result = 0;
			auto tcp    = std::make_unique<uv_tcp_t>();
			auto gcZone = hx::AutoGCZone();

			if ((result = uv_tcp_init(loop, tcp.get())) < 0)
			{
				Dynamic(cbFailure.rooted)(hx::asys::libuv::uv_err_to_enum(result));

				return;
			}

			if ((result = uv_tcp_keepalive(tcp.get(), keepAlive.has_value(), keepAlive.value_or(0))) < 0)
			{
				ConnectFailedRequest::cleanup(std::move(tcp), result, cbSuccess.rooted, cbFailure.rooted);

				return;
			}

			if (sendBuffer.has_value())
			{
				auto value = sendBuffer.value();
				if ((result = uv_send_buffer_size(reinterpret_cast<uv_handle_t*>(tcp.get()), &value)) < 0)
				{
					ConnectFailedRequest::cleanup(std::move(tcp), result, cbSuccess.rooted, cbFailure.rooted);

					return;
				}
			}

			if (recvBuffer.has_value())
			{
				auto value = recvBuffer.value();
				if ((result = uv_recv_buffer_size(reinterpret_cast<uv_handle_t*>(tcp.get()), &value)) < 0)
				{
					ConnectFailedRequest::cleanup(std::move(tcp), result, cbSuccess.rooted, cbFailure.rooted);

					return;
				}
			}

			auto request = std::make_unique<ConnectRequest>(cbSuccess.rooted, cbFailure.rooted, std::move(tcp));
			if ((result = uv_tcp_connect(&request->connect, request->tcp.get(), address(), ConnectRequest::onCallback)) < 0)
			{
				ConnectFailedRequest::cleanup(std::move(request->tcp), result, cbSuccess.rooted, cbFailure.rooted);

				return;
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

hx::asys::libuv::net::LibuvTcpSocket::LibuvTcpSocket(std::unique_ptr<uv_tcp_t> _tcp, ::hx::Anon _localAddress, ::hx::Anon _remoteAddress)
	: ctx(new Ctx(std::move(_tcp), onAlloc, onRead))
{
	HX_OBJ_WB_NEW_MARKED_OBJECT(this);

	reader        = hx::asys::Readable(new hx::asys::libuv::stream::StreamReader_obj(ctx->stream));
	writer        = hx::asys::Writable(new hx::asys::libuv::stream::StreamWriter_obj(reinterpret_cast<uv_stream_t*>(ctx->tcp.get())));
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
	class CloseRequest final : public hx::asys::libuv::BaseRequest
	{
	public:
		std::unique_ptr<uv_tcp_t> tcp;

		uv_shutdown_t shutdown;

		CloseRequest(Dynamic _cbSuccess, Dynamic _cbFailure, std::unique_ptr<uv_tcp_t> _tcp)
			: BaseRequest(_cbSuccess, _cbFailure)
			, tcp(std::move(_tcp))
		{
			tcp->data = shutdown.data = this;
		}

		static void onShutdownCallback(uv_shutdown_t* shutdown, int status)
		{
			auto request = static_cast<CloseRequest*>(shutdown->data);

			uv_close(reinterpret_cast<uv_handle_t*>(request->tcp.get()), onCloseCallback);
		}

		static void onCloseCallback(uv_handle_t* handle)
		{
			auto tcp     = reinterpret_cast<uv_tcp_t*>(handle);
			auto gcZone  = hx::AutoGCZone();
			auto request = std::unique_ptr<CloseRequest>(reinterpret_cast<CloseRequest*>(tcp->data));

			Dynamic(request->cbSuccess.rooted)();
		}
	};

	class CloseWork final : public hx::asys::libuv::WorkRequest
	{
		std::unique_ptr<uv_tcp_t> tcp;

	public:
		CloseWork(Dynamic _cbSuccess, Dynamic _cbFailure, std::unique_ptr<uv_tcp_t> _tcp) : WorkRequest(_cbSuccess, _cbFailure), tcp(std::move(_tcp)) {}

		void run(uv_loop_t* loop) override
		{
			auto gcZone  = hx::AutoGCZone();
			auto request = std::make_unique<CloseRequest>(cbSuccess.rooted, cbFailure.rooted, std::move(tcp));
			auto result  = uv_shutdown(&request->shutdown, reinterpret_cast<uv_stream_t*>(request->tcp.get()), CloseRequest::onShutdownCallback);
			if (result < 0)
			{

				Dynamic(cbFailure.rooted)(hx::asys::libuv::uv_err_to_enum(result));
			}
			else
			{
				request.release();
			}
		}
	};

	auto expected = false;
	if (false == ctx->closed.compare_exchange_strong(expected, true))
	{
		cbSuccess();

		return;
	}

	auto libuv = static_cast<LibuvAsysContext_obj::Ctx*>(ctx->tcp->loop->data);

	libuv->enqueue(std::make_unique<CloseWork>(cbSuccess.mPtr, cbFailure.mPtr, std::move(ctx->tcp)));
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
			return reinterpret_cast<sockaddr*>(&addr);
		}

	public:
		sockaddr_in addr;

		Ipv4TcpConnectWork(Dynamic _cbSuccess, Dynamic _cbFailure, std::optional<int> _keepAlive, std::optional<int> _sendBuffer, std::optional<int> _recvBuffer)
			:TcpConnectWork(_cbSuccess, _cbFailure, _keepAlive, _sendBuffer, _recvBuffer)
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
	auto work   = std::make_unique<Ipv4TcpConnectWork>(cbSuccess, cbFailure, keepAlive, sendBuffer, recvBuffer);
	auto result = uv_ip4_addr(host.utf8_str(&buffer), port, &work->addr);

	if (result < 0)
	{
		cbFailure(hx::asys::libuv::uv_err_to_enum(result));

		return;
	}
	
	libuv->ctx->enqueue(std::move(work));
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

hx::asys::libuv::net::LibuvTcpSocket::Ctx::Ctx(std::unique_ptr<uv_tcp_t> _tcp, uv_alloc_cb _cbAlloc, uv_read_cb _cbRead)
	: tcp(std::move(_tcp))
	, stream(reinterpret_cast<uv_stream_t*>(tcp.get()), _cbAlloc, _cbRead)
{
	tcp->data = this;
}