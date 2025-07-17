#include <hxcpp.h>
#include <string>
#include "LibuvChildProcess.h"
#include "LibuvCurrentProcess.h"
#include "../filesystem/LibuvFile.h"

namespace
{
    void getCwd(const char* &output, hx::Anon options)
    {
        if (null() == options)
        {
            return;
        }

        auto field = options->__Field(HX_CSTRING("cwd"), HX_PROP_DYNAMIC);
        if (field.isNull())
        {
            return;
        }

        output = field.asString().utf8_str();
    }

    void getArguments(std::vector<char*> &arguments, String command, hx::Anon options)
    {
        arguments.push_back(const_cast<char*>(command.utf8_str()));
        arguments.push_back(nullptr);

        if (null() == options)
        {
            return;
        }

        auto field = options->__Field(HX_CSTRING("args"), HX_PROP_DYNAMIC);
        if (field.isNull())
        {
            return;
        }

        auto hxArguments = Dynamic(field.asDynamic()).StaticCast<Array<String>>();
        if (null() == hxArguments)
        {
            return;
        }

        arguments.resize(static_cast<size_t>(hxArguments->length) + 2);

        for (auto i = size_t(0); i < hxArguments->length; i++)
        {
            arguments.at(1 + i) = const_cast<char*>(hxArguments[i].utf8_str());
        }
    }

    void getEnvironment(std::vector<char*>& environment, hx::Anon options)
    {
        if (null() == options)
        {
            return;
        }

        auto field = options->__Field(HX_CSTRING("env"), HX_PROP_DYNAMIC);
        if (field.isNull())
        {
            return;
        }

        auto hash = Dynamic(field.asDynamic()->__Field(HX_CSTRING("h"), HX_PROP_DYNAMIC));
        auto keys = __string_hash_keys(hash);

        environment.resize(static_cast<size_t>(keys->length) + 1);

        for (auto i = size_t(0); i < keys->length; i++)
        {
            auto& key   = keys[i];
            auto  value = __string_hash_get_string(hash, key);

            if (null() == value)
            {
                environment.at(i) = const_cast<char*>(key.utf8_str());
            }
            else
            {
                environment.at(i) = const_cast<char*>((key + HX_CSTRING("=") + value).c_str());
            }
        }
    }

    hx::asys::Writable getWritablePipe(uv_loop_t* loop, uv_stdio_container_t& container, hx::asys::libuv::system::LibuvChildProcess::Stream& stream)
    {
        /*auto result = 0;

        if ((result = uv_pipe_init(loop, &stream.pipe, 0)) < 0)
        {
            hx::Throw(HX_CSTRING("Failed to init pipe"));
        }

        container.flags       = static_cast<uv_stdio_flags>(UV_CREATE_PIPE | UV_READABLE_PIPE);
        container.data.stream = reinterpret_cast<uv_stream_t*>(&stream.pipe);

        return new hx::asys::libuv::stream::StreamWriter_obj(reinterpret_cast<uv_stream_t*>(&stream.pipe));*/
    }

    hx::asys::Readable getReadablePipe(uv_loop_t* loop, uv_stdio_container_t& container, hx::asys::libuv::system::LibuvChildProcess::Stream& stream)
    {
        /*auto result = 0;

        if ((result = uv_pipe_init(loop, &stream.pipe, 0)) < 0)
        {
            hx::Throw(HX_CSTRING("Failed to init pipe"));
        }*/

        /*container.flags       = static_cast<uv_stdio_flags>(UV_CREATE_PIPE | UV_WRITABLE_PIPE);
        container.data.stream = reinterpret_cast<uv_stream_t*>(&stream.pipe);*/

        //return new hx::asys::libuv::stream::StreamReader_obj(&stream.reader, onAlloc, onRead);
    }

    void getStdioWritable(hx::EnumBase field, uv_stdio_container_t& container, hx::asys::libuv::system::LibuvChildProcess::Stream& stream, const int index)
    {
        switch (field->_hx_getIndex())
        {
        case 1:
        {
            container.flags = static_cast<uv_stdio_flags>(UV_CREATE_PIPE | UV_READABLE_PIPE);
            container.data.stream = reinterpret_cast<uv_stream_t*>(&stream.pipe);
            break;
        }
        case 3:
        {
            container.flags = UV_INHERIT_FD;
            container.data.fd = index;
            break;
        }

        case 4:
        {
            container.flags = UV_IGNORE;
            break;
        }

        default:
            hx::Throw(HX_CSTRING("Invalid stdio option"));
        }
    }

    void getStdioReadable(hx::EnumBase field, uv_stdio_container_t& container, hx::asys::libuv::system::LibuvChildProcess::Stream& stream, int index)
    {
        switch (field->_hx_getIndex())
        {
        case 0:
        {
            container.flags = static_cast<uv_stdio_flags>(UV_CREATE_PIPE | UV_WRITABLE_PIPE);
            container.data.stream = reinterpret_cast<uv_stream_t*>(&stream.pipe);
            break;
        }case 3:
        {
            container.flags = UV_INHERIT_FD;
            container.data.fd = index;
            break;
        }

        case 4:
        {
            container.flags = UV_IGNORE;
            break;
        }
        default:
            hx::Throw(HX_CSTRING("Invalid stdio option"));
        }
    }
}

void hx::asys::system::Process_obj::open(Context ctx, String command, hx::Anon options, Dynamic cbSuccess, Dynamic cbFailure)
{
    auto uvContext = hx::asys::libuv::context(ctx);
    auto process   = std::make_unique<hx::asys::libuv::system::LibuvChildProcess::Ctx>();

    getCwd(process->options.cwd, options);
    getArguments(process->arguments, command, options);
    getEnvironment(process->environment, options);

    if (hx::IsNotNull(options))
    {
        auto io = hx::Anon(options->__Field(HX_CSTRING("stdio"), HX_PROP_DYNAMIC));

        getStdioWritable(io->__Field(HX_CSTRING("stdin"), HX_PROP_DYNAMIC), process->containers.at(0), process->streams.at(0), 0);
        getStdioReadable(io->__Field(HX_CSTRING("stdout"), HX_PROP_DYNAMIC), process->containers.at(1), process->streams.at(1), 1);
        getStdioReadable(io->__Field(HX_CSTRING("stderr"), HX_PROP_DYNAMIC), process->containers.at(2), process->streams.at(2), 2);
    }

    process->options.args        = process->arguments.data();
    process->options.env         = process->environment.empty() ? nullptr : process->environment.data();
    process->options.stdio       = process->containers.data();
    process->options.stdio_count = process->containers.size();
    process->options.file        = command.utf8_str();
    process->options.exit_cb     = [](uv_process_t* request, int64_t status, int signal) {
        auto gcZone  = hx::AutoGCZone();
        auto process = reinterpret_cast<hx::asys::libuv::system::LibuvChildProcess::Ctx*>(request->data);

        process->currentExitCode = status;

        for (auto&& callbacks : process->exitCallbacks)
        {
            callbacks->succeed(status);
        }

        process->exitCallbacks.clear();
    };

#if HX_WINDOWS
    process->options.flags |= UV_PROCESS_WINDOWS_FILE_PATH_EXACT_NAME;
#endif

    auto uidOption = options->__Field(HX_CSTRING("user"), HX_PROP_DYNAMIC);
    if (!uidOption.isNull())
    {
        process->options.flags |= UV_PROCESS_SETUID;
        process->options.uid = uidOption.asInt();
    }

    auto gidOption = options->__Field(HX_CSTRING("group"), HX_PROP_DYNAMIC);
    if (!gidOption.isNull())
    {
        process->options.flags |= UV_PROCESS_SETGID;
        process->options.gid = gidOption.asInt();
    }

    auto detachedOption = options->__Field(HX_CSTRING("detached"), HX_PROP_DYNAMIC);
    if (!detachedOption.isNull())
    {
        process->options.flags |= UV_PROCESS_DETACHED;
    }

    class OpenWork : public hx::asys::libuv::CallbackWorkRequest
    {
        std::unique_ptr<hx::asys::libuv::system::LibuvChildProcess::Ctx> ctx;

    public:
        OpenWork(Dynamic _cbSuccess, Dynamic _cbFailure, std::unique_ptr<hx::asys::libuv::system::LibuvChildProcess::Ctx> _ctx)
            : CallbackWorkRequest(_cbSuccess, _cbFailure)
            , ctx(std::move(_ctx)) {}

        void run(uv_loop_t* loop) override
        {
            auto gcZone = hx::AutoGCZone();
            auto result = 0;

            hx::asys::Writable stdinWriter;
            hx::asys::Readable stdoutReader;
            hx::asys::Readable stderrReader;

            for (auto i = 0; i < ctx->containers.size(); i++)
            {
                if (ctx->containers[i].flags & UV_CREATE_PIPE)
                {
                    if ((result = uv_pipe_init(loop, &ctx->streams[i].pipe, false)) < 0)
                    {
                        callbacks->fail(hx::asys::libuv::uv_err_to_enum(result));

                        return;
                    }
                }
            }

            if (ctx->containers[0].flags & UV_CREATE_PIPE)
            {
                stdinWriter.mPtr = new hx::asys::libuv::stream::StreamWriter_obj(reinterpret_cast<uv_stream_t*>(&ctx->streams[0].pipe));
            }
            if (ctx->containers[1].flags & UV_CREATE_PIPE)
            {
                stdoutReader.mPtr = new hx::asys::libuv::stream::StreamReader_obj(ctx->streams[1].reader);
            }
            if (ctx->containers[2].flags & UV_CREATE_PIPE)
            {
                stderrReader.mPtr = new hx::asys::libuv::stream::StreamReader_obj(ctx->streams[2].reader);
            }

            if ((result = uv_spawn(loop, &ctx->request, &ctx->options)))
            {
                callbacks->fail(hx::asys::libuv::uv_err_to_enum(result));
            }
            else
            {
                callbacks->succeed(ChildProcess(new hx::asys::libuv::system::LibuvChildProcess(ctx.release(), stdinWriter, stdoutReader, stderrReader)));
            }
        }
    };

    auto libuv = hx::asys::libuv::context(ctx);

    libuv->ctx->emplace<OpenWork>(cbSuccess, cbFailure, std::move(process));
}
