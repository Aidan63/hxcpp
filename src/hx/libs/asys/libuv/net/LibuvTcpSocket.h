#include <hxcpp.h>
#include "../stream/StreamReader.h"

namespace hx::asys::libuv::net
{
	const int KEEP_ALIVE_VALUE = 5;

	struct LibuvTcpSocketImpl final : public hx::asys::libuv::stream::StreamReader
	{
		uv_tcp_t tcp;
		int keepAlive;

		LibuvTcpSocketImpl() : hx::asys::libuv::stream::StreamReader(reinterpret_cast<uv_stream_t*>(&tcp)), keepAlive(KEEP_ALIVE_VALUE)
		{
			tcp.data = reinterpret_cast<hx::asys::libuv::stream::StreamReader*>(this);
		}
	};

	class LibuvTcpSocket final : public hx::asys::net::TcpSocket_obj
	{
		LibuvTcpSocketImpl* socket;

	public:
		LibuvTcpSocket(LibuvTcpSocketImpl* _socket);

		void getKeepAlive(Dynamic cbSuccess, Dynamic cbFailure) override;
		void getSendBufferSize(Dynamic cbSuccess, Dynamic cbFailure) override;
		void getRecvBufferSize(Dynamic cbSuccess, Dynamic cbFailure) override;

		void setKeepAlive(bool keepAlive, Dynamic cbSuccess, Dynamic cbFailure) override;
		void setSendBufferSize(int size, Dynamic cbSuccess, Dynamic cbFailure) override;
		void setRecvBufferSize(int size, Dynamic cbSuccess, Dynamic cbFailure) override;

		void read(Array<uint8_t> output, int offset, int length, Dynamic cbSuccess, Dynamic cbFailure) override;
		void write(Array<uint8_t> data, int offset, int length, Dynamic cbSuccess, Dynamic cbFailure) override;
		void flush(Dynamic cbSuccess, Dynamic cbFailure) override;
		void close(Dynamic cbSuccess, Dynamic cbFailure) override;

		void __Mark(hx::MarkContext* __inCtx) override;
#if HXCPP_VISIT_ALLOCS
		void __Visit(hx::VisitContext* __inCtx) override;
#endif
	};
}