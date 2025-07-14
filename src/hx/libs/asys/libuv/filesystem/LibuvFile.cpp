#include <hxcpp.h>
#include <memory>
#include <filesystem>
#include <cstring>
#include <limits>
#include "LibuvFile.h"
#include "FsRequest.h"

namespace
{
    int openFlag(int flag)
    {
        switch (flag)
        {
        case 0:
            return O_WRONLY | O_APPEND | O_CREAT;
        case 1:
            return O_RDONLY;
        case 2:
            return O_RDWR;
        case 3:
            return O_WRONLY | O_CREAT | O_TRUNC;
        case 4:
            return O_WRONLY | O_CREAT | O_EXCL;
        case 5:
            return O_RDWR | O_CREAT | O_TRUNC;
        case 6:
            return O_RDWR | O_CREAT | O_EXCL;
        case 7:
            return O_WRONLY | O_CREAT;
        case 8:
            return O_RDWR | O_CREAT;
        default:
            hx::Throw(HX_CSTRING("Unknown open flag"));

            return 0;
        }
    }

    int openMode(int flag)
    {
        switch (flag)
        {
        case 0:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
            return 420;
        default:
            return 0;
        }
    }

    void onOpenCallback(uv_fs_t* request)
    {
        auto gcZone    = hx::AutoGCZone();
        auto spRequest = std::unique_ptr<hx::asys::libuv::filesystem::FsRequest>(static_cast<hx::asys::libuv::filesystem::FsRequest*>(request->data));

        if (spRequest->uv.result < 0)
        {
            Dynamic(spRequest->cbFailure.rooted)(hx::asys::libuv::uv_err_to_enum(spRequest->uv.result));
        }
        else
        {
            Dynamic(spRequest->cbSuccess.rooted)(hx::asys::filesystem::File(new hx::asys::libuv::filesystem::LibuvFile_obj(spRequest->uv.loop, spRequest->uv.result, String::create(spRequest->uv.path))));
        }
    }

    void onStatCallback(uv_fs_t* request)
    {
        auto gcZone    = hx::AutoGCZone();
        auto spRequest = std::unique_ptr<hx::asys::libuv::filesystem::FsRequest>(static_cast<hx::asys::libuv::filesystem::FsRequest*>(request->data));

        if (spRequest->uv.result < 0)
        {
            Dynamic(spRequest->cbFailure.rooted)(hx::asys::libuv::uv_err_to_enum(spRequest->uv.result));
        }
        else
        {
            auto statBuf = hx::Anon_obj::Create();
            statBuf->__SetField(HX_CSTRING("atime"), static_cast<int>(spRequest->uv.statbuf.st_atim.tv_sec), hx::PropertyAccess::paccDynamic);
            statBuf->__SetField(HX_CSTRING("mtime"), static_cast<int>(spRequest->uv.statbuf.st_mtim.tv_sec), hx::PropertyAccess::paccDynamic);
            statBuf->__SetField(HX_CSTRING("ctime"), static_cast<int>(spRequest->uv.statbuf.st_ctim.tv_sec), hx::PropertyAccess::paccDynamic);
            statBuf->__SetField(HX_CSTRING("dev"), static_cast<int>(spRequest->uv.statbuf.st_dev), hx::PropertyAccess::paccDynamic);
            statBuf->__SetField(HX_CSTRING("uid"), static_cast<int>(spRequest->uv.statbuf.st_uid), hx::PropertyAccess::paccDynamic);
            statBuf->__SetField(HX_CSTRING("gid"), static_cast<int>(spRequest->uv.statbuf.st_gid), hx::PropertyAccess::paccDynamic);
            statBuf->__SetField(HX_CSTRING("ino"), static_cast<int>(spRequest->uv.statbuf.st_ino), hx::PropertyAccess::paccDynamic);
            statBuf->__SetField(HX_CSTRING("mode"), static_cast<int>(spRequest->uv.statbuf.st_mode), hx::PropertyAccess::paccDynamic);
            statBuf->__SetField(HX_CSTRING("nlink"), static_cast<int>(spRequest->uv.statbuf.st_nlink), hx::PropertyAccess::paccDynamic);
            statBuf->__SetField(HX_CSTRING("rdev"), static_cast<int>(spRequest->uv.statbuf.st_rdev), hx::PropertyAccess::paccDynamic);
            statBuf->__SetField(HX_CSTRING("size"), static_cast<int>(spRequest->uv.statbuf.st_size), hx::PropertyAccess::paccDynamic);
            statBuf->__SetField(HX_CSTRING("blksize"), static_cast<int>(spRequest->uv.statbuf.st_blksize), hx::PropertyAccess::paccDynamic);
            statBuf->__SetField(HX_CSTRING("blocks"), static_cast<int>(spRequest->uv.statbuf.st_blocks), hx::PropertyAccess::paccDynamic);

            Dynamic(spRequest->cbSuccess.rooted)(statBuf);
        }
    }

    void onFinalise(Dynamic obj)
    {
        //class FinaliseWork : public hx::asys::libuv::WorkRequest
        //{
        //public:
        //    void run(uv_loop_t* loop) override
        //    {
        //        //
        //    }
        //};

        //auto file = reinterpret_cast<hx::asys::libuv::filesystem::LibuvFile_obj*>(obj.mPtr);
        //auto ctx  = static_cast<hx::asys::libuv::LibuvAsysContext_obj::Ctx*>();
    }
}

void hx::asys::filesystem::File_obj::open(Context ctx, String path, int flags, Dynamic cbSuccess, Dynamic cbFailure)
{
    class FileOpenRequest : public hx::asys::libuv::filesystem::FsRequest
    {
        std::unique_ptr<hx::strbuf> pathBuffer;

    public:
        FileOpenRequest(Dynamic _cbSuccess, Dynamic _cbFailure, std::unique_ptr<hx::strbuf> _pathBuffer)
            : FsRequest(_cbSuccess, _cbFailure)
            , pathBuffer(std::move(_pathBuffer)) {}
    };

    class FileOpenWork : public hx::asys::libuv::CallbackWorkRequest
    {
        const int flags;
        const int mode;
        std::unique_ptr<hx::strbuf> pathBuffer;
        const char* path;

    public:
        FileOpenWork(Dynamic _cbSuccess, Dynamic _cbFailure, String _path, int _flags, int _mode)
            : CallbackWorkRequest(_cbSuccess, _cbFailure)
            , pathBuffer(new hx::strbuf())
            , flags(_flags)
            , mode(_mode)
            , path(_path.utf8_str(pathBuffer.get()))
        {
            //
        }

        void run(uv_loop_t* loop) override
        {
            auto gcZone  = hx::AutoGCZone();
            auto request = std::make_unique<FileOpenRequest>(callbacks->cbSuccess.rooted, callbacks->cbFailure.rooted, std::move(pathBuffer));
            auto result  = uv_fs_open(loop, &request->uv, path, flags, mode, onOpenCallback);
            if (result < 0)
            {
                callbacks->fail(hx::asys::libuv::uv_err_to_enum(result));
            }
            else
            {
                request.release();
            }
        }
    };

    auto libuvCtx = hx::asys::libuv::context(ctx);

    libuvCtx->ctx->emplace<FileOpenWork>(cbSuccess, cbFailure, path, openFlag(flags), openMode(flags));
}

void hx::asys::filesystem::File_obj::temp(Context ctx, Dynamic cbSuccess, Dynamic cbFailure)
{
    class TempWork : public hx::asys::libuv::CallbackWorkRequest
    {
        std::filesystem::path path;

    public:
        TempWork(Dynamic _cbSuccess, Dynamic _cbFailure, std::filesystem::path _path)
            : CallbackWorkRequest(_cbSuccess, _cbFailure)
            , path(_path) {}

        void run(uv_loop_t* loop) override
        {
            auto gcZone  = hx::AutoGCZone();
            auto request = std::make_unique<hx::asys::libuv::filesystem::FsRequest>(callbacks->cbSuccess.rooted, callbacks->cbFailure.rooted);
            auto result  = uv_fs_mkstemp(loop, &request->uv, path.string().c_str(), onOpenCallback);
            if (result < 0)
            {
                callbacks->fail(hx::asys::libuv::uv_err_to_enum(result));
            }
            else
            {
                request.release();
            }
        }
    };

    auto size     = size_t{ 1 };
    auto nullchar = '\0';
    auto result   = uv_os_tmpdir(&nullchar, &size);
    if (result < 0 && result != UV_ENOBUFS)
    {
        cbFailure(hx::asys::libuv::uv_err_to_enum(result));

        return;
    }

    auto buffer = std::vector<char>(size);
    result = uv_os_tmpdir(buffer.data(), &size);
    if (result < 0)
    {
        cbFailure(hx::asys::libuv::uv_err_to_enum(result));

        return;
    }

    auto path     = std::filesystem::path(buffer.data()) / std::filesystem::path("XXXXXX");
    auto libuvCtx = hx::asys::libuv::context(ctx);

    libuvCtx->ctx->emplace<TempWork>(cbSuccess, cbFailure, path);
}

void hx::asys::filesystem::File_obj::info(Context ctx, String path, Dynamic cbSuccess, Dynamic cbFailure)
{
    class InfoRequest final : public hx::asys::libuv::filesystem::FsRequest
    {
        std::unique_ptr<hx::strbuf> buffer;

    public:
        InfoRequest(Dynamic _cbSuccess, Dynamic _cbFailure, std::unique_ptr<hx::strbuf> _buffer)
            : FsRequest(_cbSuccess, _cbFailure)
            , buffer(std::move(_buffer)) {}
    };

    class InfoWork final : public hx::asys::libuv::CallbackWorkRequest
    {
        std::unique_ptr<hx::strbuf> buffer;
        const char* path;

    public:
        InfoWork(Dynamic _cbSuccess, Dynamic _cbFailure, std::unique_ptr<hx::strbuf> _buffer, const char* _path)
            : CallbackWorkRequest(_cbSuccess, _cbFailure)
            , buffer(std::move(_buffer))
            , path(_path) {}

        void run(uv_loop_t* loop) override
        {
            auto gcZone  = hx::AutoGCZone();
            auto request = std::make_unique<InfoRequest>(callbacks->cbSuccess.rooted, callbacks->cbFailure.rooted, std::move(buffer));
            auto result  = uv_fs_stat(loop, &request->uv, path, onStatCallback);
            if (result < 0)
            {
                callbacks->fail(hx::asys::libuv::uv_err_to_enum(result));
            }
            else
            {
                request.release();
            }
        }
    };

    auto libuvCtx   = hx::asys::libuv::context(ctx);
    auto pathBuffer = std::make_unique<hx::strbuf>();
    auto pathString = path.utf8_str(pathBuffer.get());

    libuvCtx->ctx->emplace<InfoWork>(cbSuccess, cbFailure, std::move(pathBuffer), pathString);
}

hx::asys::libuv::filesystem::LibuvFile_obj::LibuvFile_obj(uv_loop_t* _loop, uv_file _file, const String _path)
    : File_obj(_path)
    , loop(_loop)
    , closed(false)
    , file(_file)
{
    _hx_set_finalizer(this, onFinalise);
}

void hx::asys::libuv::filesystem::LibuvFile_obj::write(::cpp::Int64 pos, Array<uint8_t> data, int offset, int length, Dynamic cbSuccess, Dynamic cbFailure)
{
    struct WriteRequest final : public hx::asys::libuv::filesystem::FsRequest
    {
    private:
        std::unique_ptr<hx::ArrayPin> pin;

    public:
        const uv_buf_t buffer;

        WriteRequest(std::unique_ptr<hx::ArrayPin> _pin, const int _offset, const int _length, Dynamic _cbSuccess, Dynamic _cbFailure)
            : FsRequest(_cbSuccess, _cbFailure)
            , pin(std::move(_pin))
            , buffer(uv_buf_init(pin->GetBase() + _offset, _length))
        {
            //
        }
    };

    class WriteWork final : public hx::asys::libuv::CallbackWorkRequest
    {
        std::unique_ptr<hx::ArrayPin> pin;
        const uv_file file;
        const int64_t pos;
        const int offset;
        const int length;

    public:
        WriteWork(Dynamic _cbSuccess, Dynamic _cbFailure, hx::ArrayPin* _pin, uv_file _file, int64_t _pos, int _offset, int _length)
            : CallbackWorkRequest(_cbSuccess, _cbFailure)
            , pin(_pin)
            , file(_file)
            , pos(_pos)
            , offset(_offset)
            , length(_length) {}

        void run(uv_loop_t* loop) override
        {
            auto gcZone  = hx::AutoGCZone();
            auto request = std::make_unique<WriteRequest>(std::move(pin), offset, length, callbacks->cbSuccess.rooted, callbacks->cbFailure.rooted);
            auto result  = uv_fs_write(loop, &request->uv, file, &request->buffer, 1, pos, FsRequest::callback);
            if (result < 0)
            {
                callbacks->fail(hx::asys::libuv::uv_err_to_enum(result));
            }
            else
            {
                request.release();
            }
        }
    };

    auto ctx = static_cast<LibuvAsysContext_obj::Ctx*>(loop->data);

    ctx->emplace<WriteWork>(cbSuccess, cbFailure, data->Pin(), file, pos, offset, length);
}

void hx::asys::libuv::filesystem::LibuvFile_obj::read(::cpp::Int64 pos, Array<uint8_t> output, int offset, int length, Dynamic cbSuccess, Dynamic cbFailure)
{
    struct ReadRequest final : public hx::asys::libuv::filesystem::FsRequest
    {
    private:
        std::unique_ptr<hx::ArrayPin> pin;

    public:
        const uv_buf_t buffer;

        ReadRequest(std::unique_ptr<hx::ArrayPin> _pin, const int _offset, const int _length, Dynamic _cbSuccess, Dynamic _cbFailure)
            : FsRequest(_cbSuccess, _cbFailure)
            , pin(std::move(_pin))
            , buffer(uv_buf_init(pin->GetBase() + _offset, _length))
        {
            //
        }
    };

    class ReadWork final : public hx::asys::libuv::CallbackWorkRequest
    {
        std::unique_ptr<hx::ArrayPin> pin;
        const uv_file file;
        const int64_t pos;
        const int offset;
        const int length;

    public:
        ReadWork(Dynamic _cbSuccess, Dynamic _cbFailure, hx::ArrayPin* _pin, uv_file _file, int64_t _pos, int _offset, int _length)
            : CallbackWorkRequest(_cbSuccess, _cbFailure)
            , pin(_pin)
            , file(_file)
            , pos(_pos)
            , offset(_offset)
            , length(_length) {}

        void run(uv_loop_t* loop) override
        {
            auto gcZone  = hx::AutoGCZone();
            auto request = std::make_unique<ReadRequest>(std::move(pin), offset, length, callbacks->cbSuccess.rooted, callbacks->cbFailure.rooted);
            auto result  = uv_fs_read(loop, &request->uv, file, &request->buffer, 1, pos, FsRequest::callback);
            if (result < 0)
            {
                callbacks->fail(hx::asys::libuv::uv_err_to_enum(result));
            }
            else
            {
                request.release();
            }
        }
    };

    auto ctx = static_cast<LibuvAsysContext_obj::Ctx*>(loop->data);

    ctx->emplace<ReadWork>(cbSuccess, cbFailure, output->Pin(), file, pos, offset, length);
}

void hx::asys::libuv::filesystem::LibuvFile_obj::info(Dynamic cbSuccess, Dynamic cbFailure)
{
    class StatWork final : public hx::asys::libuv::CallbackWorkRequest
    {
        const uv_file file;

    public:
        StatWork(Dynamic _cbSuccess, Dynamic _cbFailure, uv_file _file)
            : CallbackWorkRequest(_cbSuccess, _cbFailure)
            , file(_file)
        {
            //
        }

        void run(uv_loop_t* loop) override
        {
            auto gcZone  = hx::AutoGCZone();
            auto request = std::make_unique<hx::asys::libuv::filesystem::FsRequest>(callbacks->cbSuccess.rooted, callbacks->cbFailure.rooted);
            auto result  = uv_fs_fstat(loop, &request->uv, file, onStatCallback);
            if (result < 0)
            {
                callbacks->fail(hx::asys::libuv::uv_err_to_enum(result));
            }
            else
            {
                request.release();
            }
        }
    };

    auto ctx = static_cast<LibuvAsysContext_obj::Ctx*>(loop->data);

    ctx->emplace<StatWork>(cbSuccess, cbFailure, file);
}

void hx::asys::libuv::filesystem::LibuvFile_obj::resize(int size, Dynamic cbSuccess, Dynamic cbFailure)
{
    class ResizeWork final : public hx::asys::libuv::CallbackWorkRequest
    {
        const uv_file file;
        const int size;

    public:
        ResizeWork(const uv_file _file, const int _size, Dynamic _cbSuccess, Dynamic _cbFailure)
            : CallbackWorkRequest(_cbSuccess, _cbFailure)
            , file(_file)
            , size(_size) {}

        void run(uv_loop_t* loop) override
        {
            auto gcZone  = hx::AutoGCZone();
            auto request = std::make_unique<FsRequest>(callbacks->cbSuccess.rooted, callbacks->cbFailure.rooted);
            auto result  = uv_fs_ftruncate(loop, &request->uv, file, size, FsRequest::callback);
            if (result < 0)
            {
                callbacks->fail(hx::asys::libuv::uv_err_to_enum(result));
            }
            else
            {
                request.release();
            }
        }
    };

    auto ctx = static_cast<LibuvAsysContext_obj::Ctx*>(loop->data);

    ctx->emplace<ResizeWork>(file, size, cbSuccess.mPtr, cbFailure.mPtr);
}

void hx::asys::libuv::filesystem::LibuvFile_obj::setPermissions(int permissions, Dynamic cbSuccess, Dynamic cbFailure)
{
    class SetPermissionsWork final : public hx::asys::libuv::CallbackWorkRequest
    {
        const uv_file file;
        const int permissions;

    public:
        SetPermissionsWork(const uv_file _file, const int _permissions, Dynamic _cbSuccess, Dynamic _cbFailure)
            : CallbackWorkRequest(_cbSuccess, _cbFailure)
            , file(_file)
            , permissions(_permissions) {}

        void run(uv_loop_t* loop) override
        {
            auto gcZone  = hx::AutoGCZone();
            auto request = std::make_unique<FsRequest>(callbacks->cbSuccess.rooted, callbacks->cbFailure.rooted);
            auto result  = uv_fs_fchmod(loop, &request->uv, file, permissions, FsRequest::callback);
            if (result < 0)
            {
                callbacks->fail(hx::asys::libuv::uv_err_to_enum(result));
            }
            else
            {
                request.release();
            }
        }
    };

    auto ctx = static_cast<LibuvAsysContext_obj::Ctx*>(loop->data);

    ctx->emplace<SetPermissionsWork>(file, permissions, cbSuccess.mPtr, cbFailure.mPtr);
}

void hx::asys::libuv::filesystem::LibuvFile_obj::setOwner(int user, int group, Dynamic cbSuccess, Dynamic cbFailure)
{
    class SetOwnerWork final : public hx::asys::libuv::CallbackWorkRequest
    {
        const uv_file file;
        const int user;
        const int group;

    public:
        SetOwnerWork(const uv_file _file, const int _user, const int _group, Dynamic _cbSuccess, Dynamic _cbFailure)
            : CallbackWorkRequest(_cbSuccess, _cbFailure)
            , file(_file)
            , user(_user)
            , group(_group) {}

        void run(uv_loop_t* loop) override
        {
            auto gcZone  = hx::AutoGCZone();
            auto request = std::make_unique<FsRequest>(callbacks->cbSuccess.rooted, callbacks->cbFailure.rooted);
            auto result  = uv_fs_fchown(loop, &request->uv, file, user, group, FsRequest::callback);
            if (result < 0)
            {
                callbacks->fail(hx::asys::libuv::uv_err_to_enum(result));
            }
            else
            {
                request.release();
            }
        }
    };

    auto ctx = static_cast<LibuvAsysContext_obj::Ctx*>(loop->data);

    ctx->emplace<SetOwnerWork>(file, user, group, cbSuccess.mPtr, cbFailure.mPtr);
}

void hx::asys::libuv::filesystem::LibuvFile_obj::setTimes(int accessTime, int modificationTime, Dynamic cbSuccess, Dynamic cbFailure)
{
    class SetTimesWork final : public hx::asys::libuv::CallbackWorkRequest
    {
        const uv_file file;
        const int accessTime;
        const int modificationTime;

    public:
        SetTimesWork(const uv_file _file, const int _accessTime, const int _modificationTime, Dynamic _cbSuccess, Dynamic _cbFailure)
            : CallbackWorkRequest(_cbSuccess, _cbFailure)
            , file(_file)
            , accessTime(_accessTime)
            , modificationTime(_modificationTime) { }

        void run(uv_loop_t* loop) override
        {
            auto gcZone  = hx::AutoGCZone();
            auto request = std::make_unique<FsRequest>(callbacks->cbSuccess.rooted, callbacks->cbFailure.rooted);
            auto result  = uv_fs_futime(loop, &request->uv, file, accessTime, modificationTime, FsRequest::callback);
            if (result < 0)
            {
                callbacks->fail(hx::asys::libuv::uv_err_to_enum(result));
            }
            else
            {
                request.release();
            }
        }
    };

    auto ctx = static_cast<LibuvAsysContext_obj::Ctx*>(loop->data);

    ctx->emplace<SetTimesWork>(file, accessTime, modificationTime, cbSuccess.mPtr, cbFailure.mPtr);
}

void hx::asys::libuv::filesystem::LibuvFile_obj::flush(Dynamic cbSuccess, Dynamic cbFailure)
{
    class FlushWork final : public hx::asys::libuv::CallbackWorkRequest
    {
        const uv_file file;

    public:
        FlushWork(const uv_file _file, Dynamic _cbSuccess, Dynamic _cbFailure)
            : CallbackWorkRequest(_cbSuccess, _cbFailure)
            , file(_file) {}

        void run(uv_loop_t* loop) override
        {
            auto gcZone  = hx::AutoGCZone();
            auto request = std::make_unique<FsRequest>(callbacks->cbSuccess.rooted, callbacks->cbFailure.rooted);
            auto result  = uv_fs_fsync(loop, &request->uv, file, FsRequest::callback);
            if (result < 0)
            {
                callbacks->fail(hx::asys::libuv::uv_err_to_enum(result));
            }
            else
            {
                request.release();
            }
        }
    };

    auto ctx = static_cast<LibuvAsysContext_obj::Ctx*>(loop->data);

    ctx->emplace<FlushWork>(file, cbSuccess.mPtr, cbFailure.mPtr);
}

void hx::asys::libuv::filesystem::LibuvFile_obj::close(Dynamic cbSuccess, Dynamic cbFailure)
{
    class CloseWork final : public hx::asys::libuv::CallbackWorkRequest
    {
        const uv_file file;

    public:
        CloseWork(const uv_file _file, Dynamic _cbSuccess, Dynamic _cbFailure)
            : CallbackWorkRequest(_cbSuccess, _cbFailure)
            , file(_file) {}

        void run(uv_loop_t* loop) override
        {
            auto gcZone  = hx::AutoGCZone();
            auto request = std::make_unique<FsRequest>(callbacks->cbSuccess.rooted, callbacks->cbFailure.rooted);
            auto result  = uv_fs_close(loop, &request->uv, file, FsRequest::callback);
            if (result < 0)
            {
                callbacks->fail(hx::asys::libuv::uv_err_to_enum(result));
            }
            else
            {
                request.release();
            }
        }
    };

    auto expected = false;
    if (false == closed.compare_exchange_strong(expected, true))
    {
        cbSuccess();

        return;
    }

    _hx_set_finalizer(this, nullptr);

    auto ctx = static_cast<LibuvAsysContext_obj::Ctx*>(loop->data);

    ctx->emplace<CloseWork>(file, cbSuccess.mPtr, cbFailure.mPtr);
}

void hx::asys::libuv::filesystem::LibuvFile_obj::__Mark(hx::MarkContext* __inCtx)
{
    HX_MARK_MEMBER(path);
}

#ifdef HXCPP_VISIT_ALLOCS

void hx::asys::libuv::filesystem::LibuvFile_obj::__Visit(hx::VisitContext* __inCtx)
{
    HX_VISIT_MEMBER(path);
}

#endif