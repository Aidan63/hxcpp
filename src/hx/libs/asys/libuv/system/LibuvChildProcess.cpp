#include <hxcpp.h>
#include <vector>
#include <deque>
#include <array>
#include <memory>
#include "LibuvChildProcess.h"

namespace
{
	void onAlloc(uv_handle_t* handle, const size_t suggested, uv_buf_t* buffer)
	{
		auto  ctx = static_cast<hx::asys::libuv::system::LibuvChildProcess::Stream*>(handle->data);
		auto& staging = ctx->reader.staging.emplace_back(suggested);

		buffer->base = staging.data();
		buffer->len = staging.size();
	}

	void onRead(uv_stream_t* stream, const ssize_t len, const uv_buf_t* read)
	{
		auto  gc = hx::AutoGCZone();
		auto  ctx = static_cast<hx::asys::libuv::system::LibuvChildProcess::Stream*>(stream->data);

		if (len <= 0)
		{
			ctx->reader.reject(len);

			return;
		}

		ctx->reader.buffer.insert(ctx->reader.buffer.end(), read->base, read->base + len);
		ctx->reader.consume();
	}
}

hx::asys::libuv::system::LibuvChildProcess::Stream::Stream()
	: pipe()
	, reader(reinterpret_cast<uv_stream_t*>(&pipe), onAlloc, onRead)
{
	pipe.data = this;
}

hx::asys::libuv::system::LibuvChildProcess::Ctx::Ctx()
	: request()
	, options()
	, arguments()
	, environment()
	, containers()
	, streams()
	, currentExitCode()
	, exitCallbacks()
{
	request.data = this;
}

hx::asys::libuv::system::LibuvChildProcess::LibuvChildProcess(Ctx* ctx, Writable oStdin, Readable oStdout, Readable oStderr)
	: hx::asys::system::ChildProcess_obj(oStdin, oStdout, oStderr)
	, ctx(ctx)
{
	HX_OBJ_WB_NEW_MARKED_OBJECT(this);
}

hx::asys::Pid hx::asys::libuv::system::LibuvChildProcess::pid()
{
	return ctx->request.pid;
}
//
//void hx::asys::libuv::system::LibuvChildProcess::sendSignal(hx::EnumBase signal, Dynamic cbSuccess, Dynamic cbFailure)
//{
//	auto signum = 0;
//	switch (signal->_hx_getIndex())
//	{
//	case 0:
//		signum = SIGTERM;
//		break;
//	case 1:
//		signum = SIGKILL;
//		break;
//	case 2:
//		signum = SIGINT;
//		break;
//	case 3:
//#if HX_WINDOWS
//		cbFailure(hx::asys::libuv::uv_err_to_enum(-1));
//
//		return;
//#else
//		signum = SIGSTOP;
//#endif
//		break;
//	case 4:
//#if HX_WINDOWS
//		cbFailure(hx::asys::libuv::uv_err_to_enum(-1));
//
//		return;
//#else
//		signum = SIGCONT;
//#endif
//		break;
//	case 5:
//#if HX_WINDOWS
//		cbFailure(hx::asys::libuv::uv_err_to_enum(-1));
//
//		return;
//#else
//		signum = hx::asys::libuv::toPosixCode(signal->_hx_getInt(5));
//#endif
//		break;
//	default:
//		cbFailure(hx::asys::libuv::uv_err_to_enum(-1));
//		return;
//	}
//
//	auto result = 0;
//	if ((result = uv_process_kill(&ctx->request, signum)) < 0)
//	{
//		cbFailure(hx::asys::libuv::uv_err_to_enum(result));
//	}
//	else
//	{
//		cbSuccess();
//	}
//}

void hx::asys::libuv::system::LibuvChildProcess::exitCode(Dynamic cbSuccess, Dynamic cbFailure)
{
	class ExitCodeWork : public hx::asys::libuv::CallbackWorkRequest
	{
		Ctx* ctx;

	public:
		ExitCodeWork(Dynamic _cbSuccess, Dynamic _cbFailure, Ctx* _ctx)
			: CallbackWorkRequest(_cbSuccess, _cbFailure)
			, ctx(_ctx) { }

		void run(uv_loop_t* loop) override
		{
			auto gcZone = hx::AutoGCZone();

			if (ctx->currentExitCode.has_value())
			{
				callbacks->succeed(static_cast<int>(ctx->currentExitCode.value()));
			}
			else
			{
				ctx->exitCallbacks.push_back(std::move(callbacks));
			}
		}
	};

	auto libuvCtx = reinterpret_cast<LibuvAsysContext_obj::Ctx*>(ctx->request.loop->data);

	libuvCtx->emplace<ExitCodeWork>(cbSuccess, cbFailure, ctx);
}

void hx::asys::libuv::system::LibuvChildProcess::close(Dynamic cbSuccess, Dynamic cbFailure)
{
	uv_close(reinterpret_cast<uv_handle_t*>(&ctx->request), nullptr);

	cbSuccess();
}