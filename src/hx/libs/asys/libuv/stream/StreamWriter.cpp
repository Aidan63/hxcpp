#include <hxcpp.h>
#include "StreamWriter.h"
#include <cstring>

namespace
{
	class WriteRequest final : hx::asys::libuv::BaseRequest
	{
		std::unique_ptr<hx::ArrayPin> pin;

	public:
		uv_write_t request;
		uv_buf_t buffer;

		WriteRequest(Dynamic _cbSuccess, Dynamic _cbFailure, std::unique_ptr<hx::ArrayPin> _pin, uv_buf_t _buffer)
			: BaseRequest(_cbSuccess, _cbFailure)
			, pin(std::move(_pin))
			, buffer(_buffer)
		{
			request.data = this;
		}

		static void callback(uv_write_t* request, int status)
		{
			auto gcZone = hx::AutoGCZone();
			auto spData = std::unique_ptr<WriteRequest>(static_cast<WriteRequest*>(request->data));

			if (status < 0)
			{
				Dynamic(spData->cbFailure.rooted)(hx::asys::libuv::uv_err_to_enum(status));
			}
			else
			{
				Dynamic(spData->cbSuccess.rooted)(spData->buffer.len);
			}
		}
	};

	class WriteWork final : public hx::asys::libuv::WorkRequest
	{
		uv_stream_t* stream;
		std::unique_ptr<hx::ArrayPin> pin;
		char* base;
		int length;

	public:
		WriteWork(Dynamic _cbSuccess, Dynamic _cbFailure, uv_stream_t* _stream, hx::ArrayPin* _pin, char* _base, int _length)
			: WorkRequest(_cbSuccess, _cbFailure)
			, stream(_stream)
			, pin(_pin)
			, base(_base)
			, length(_length) { }

		void run(uv_loop_t* loop) override
		{
			auto request = std::make_unique<WriteRequest>(cbSuccess.rooted, cbFailure.rooted, std::move(pin), uv_buf_init(base, length));
			auto result  = uv_write(&request->request, stream, &request->buffer, 1, WriteRequest::callback);

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
}

hx::asys::libuv::stream::StreamWriter_obj::StreamWriter_obj(uv_stream_t* stream) : stream(stream) {}

void hx::asys::libuv::stream::StreamWriter_obj::write(Array<uint8_t> data, int offset, int length, Dynamic cbSuccess, Dynamic cbFailure)
{
	auto ctx = static_cast<LibuvAsysContext_obj::Ctx*>(stream->loop->data);

	ctx->enqueue(std::make_unique<WriteWork>(cbSuccess, cbFailure, stream, data->Pin(), data->GetBase() + offset, length));
}

void hx::asys::libuv::stream::StreamWriter_obj::flush(Dynamic cbSuccess, Dynamic cbFailure)
{
	cbSuccess();
}
