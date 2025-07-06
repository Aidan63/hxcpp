#include <hxcpp.h>
#include <memory>
#include <filesystem>
#include <functional>
#include <array>
#include "FsRequest.h"

namespace
{
    hx::asys::filesystem::FileAccessMode operator&(hx::asys::filesystem::FileAccessMode lhs, hx::asys::filesystem::FileAccessMode rhs)
    {
        return
            static_cast<hx::asys::filesystem::FileAccessMode>(
                static_cast<std::underlying_type_t<hx::asys::filesystem::FileAccessMode>>(lhs) &
                static_cast<std::underlying_type_t<hx::asys::filesystem::FileAccessMode>>(rhs));
    }

    void check_type_callback(int type, uv_fs_t* request)
    {
        auto spRequest = std::unique_ptr<hx::asys::libuv::filesystem::FsRequest>(static_cast<hx::asys::libuv::filesystem::FsRequest*>(request->data));
        auto gcZone    = hx::AutoGCZone();

        if (spRequest->uv.result == UV_ENOENT)
        {
            Dynamic(spRequest->cbSuccess.rooted)(false);
        }
        else if (spRequest->uv.result < 0)
        {
            Dynamic(spRequest->cbFailure.rooted)(hx::asys::libuv::uv_err_to_enum(spRequest->uv.result));
        }
        else
        {
            Dynamic(spRequest->cbSuccess.rooted)((spRequest->uv.statbuf.st_mode & S_IFMT) == type);
        }
    }

    void path_callback(uv_fs_t* request)
    {
        auto spRequest = std::unique_ptr<hx::asys::libuv::filesystem::FsRequest>(static_cast<hx::asys::libuv::filesystem::FsRequest*>(request->data));
        auto gcZone    = hx::AutoGCZone();

        if (spRequest->uv.result < 0)
        {
            Dynamic(spRequest->cbFailure.rooted)(hx::asys::libuv::uv_err_to_enum(spRequest->uv.result));
        }
        else
        {
            Dynamic(spRequest->cbSuccess.rooted)(String::create(static_cast<const char*>(spRequest->uv.ptr)));
        }
    }

    class NextEntriesRequest : public hx::asys::libuv::filesystem::FsRequest
    {
    public:
        std::vector<uv_dirent_t> entries;

        NextEntriesRequest(Dynamic cbSuccess, Dynamic cbFailure, const int _batch)
            : FsRequest(cbSuccess, cbFailure)
            , entries(_batch) {}
    };

    class LibuvDirectory_obj : public hx::asys::filesystem::Directory_obj
    {
    public:
        uv_loop_t* loop;
        uv_dir_t* dir;

        LibuvDirectory_obj(uv_loop_t* _loop, uv_dir_t* _dir, String _path)
            : Directory_obj(_path), loop(_loop), dir(_dir) {}

        void next(const int batch, Dynamic cbSuccess, Dynamic cbFailure)
        {
            auto wrapper = [](uv_fs_t* request) {
                auto gcZone    = hx::AutoGCZone();
                auto spRequest = static_cast<NextEntriesRequest*>(request->data);

                if (spRequest->uv.result < 0)
                {
                    Dynamic(spRequest->cbFailure.rooted)(hx::asys::libuv::uv_err_to_enum(spRequest->uv.result));
                }
                else
                {
                    auto entries = Array<String>(spRequest->uv.result, 0);

                    for (auto i = 0; i < spRequest->uv.result; i++)
                    {
                        if (nullptr != spRequest->entries.at(i).name)
                        {
                            entries[i] = String::create(spRequest->entries.at(i).name);
                        }
                    }

                    Dynamic(spRequest->cbSuccess.rooted)(entries);
                }
            };

            auto request = std::make_unique<NextEntriesRequest>(cbSuccess, cbFailure, batch);
            dir->nentries = request->entries.capacity();
            dir->dirents  = request->entries.data();

            auto result = uv_fs_readdir(loop, &request->uv, dir, wrapper);

            if (result < 0)
            {
                cbFailure(hx::asys::libuv::uv_err_to_enum(result));
            }
            else
            {
                request.release();
            }
        }

        void close(Dynamic cbSuccess, Dynamic cbFailure)
        {
            auto request = std::make_unique<hx::asys::libuv::filesystem::FsRequest>(cbSuccess, cbFailure);
            auto result  = uv_fs_closedir(loop, &request->uv, dir, hx::asys::libuv::filesystem::FsRequest::callback);

            if (result < 0)
            {
                cbFailure(hx::asys::libuv::uv_err_to_enum(result));
            }
            else
            {
                request.release();
            }
        }
    };
}

//void hx::asys::filesystem::Directory_obj::open(Context ctx, String path, Dynamic cbSuccess, Dynamic cbFailure)
//{
//    auto libuvCtx = hx::asys::libuv::context(ctx);
//    auto wrapper  = [](uv_fs_t* request) {
//        auto gcZone    = hx::AutoGCZone();
//        auto spRequest = std::unique_ptr<hx::asys::libuv::filesystem::FsRequest>(static_cast<hx::asys::libuv::filesystem::FsRequest*>(request->data));
//
//        if (spRequest->uv.result < 0)
//        {
//            Dynamic(spRequest->cbFailure.rooted)(hx::asys::libuv::uv_err_to_enum(spRequest->uv.result));
//        }
//        else
//        {
//            Dynamic(spRequest->cbSuccess.rooted)(Directory(new LibuvDirectory_obj(spRequest->uv.loop, static_cast<uv_dir_t*>(spRequest->uv.ptr), String::create(spRequest->uv.path))));
//        }
//    };
//
//    auto request = std::make_unique<hx::asys::libuv::filesystem::FsRequest>(path, cbSuccess, cbFailure);
//    auto result  = uv_fs_opendir(libuvCtx->uvLoop, &request->uv, request->path, wrapper);
//
//    if (result < 0)
//    {
//        cbFailure(hx::asys::libuv::uv_err_to_enum(result));
//    }
//    else
//    {
//        request.release();
//    }
//}

void hx::asys::filesystem::Directory_obj::create(Context ctx, String path, int permissions, Dynamic cbSuccess, Dynamic cbFailure)
{
    class CreateRequest final : public hx::asys::libuv::filesystem::FsRequest
    {
        std::unique_ptr<hx::strbuf> pathBuffer;

    public:
        CreateRequest(Dynamic _cbSuccess, Dynamic _cbFailure, std::unique_ptr<hx::strbuf> _pathBuffer)
            : FsRequest(_cbSuccess, _cbFailure)
            , pathBuffer(std::move(_pathBuffer)) {
        }
    };

    class CreateWork final : public hx::asys::libuv::WorkRequest
    {
        std::unique_ptr<hx::strbuf> pathBuffer;
        const char* path;
        const int flags;

    public:
        CreateWork(Dynamic _cbSuccess, Dynamic _cbFailure, std::unique_ptr<hx::strbuf> _pathBuffer, const char* _path, const int _flags)
            : WorkRequest(_cbSuccess, _cbFailure)
            , pathBuffer(std::move(_pathBuffer))
            , path(_path)
            , flags(_flags) { }

        void run(uv_loop_t* loop) override
        {
            auto request = std::make_unique<CreateRequest>(cbSuccess.rooted, cbFailure.rooted, std::move(pathBuffer));
            auto result  = uv_fs_mkdir(loop, &request->uv, path, flags, hx::asys::libuv::filesystem::FsRequest::callback);
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

    auto libuvCtx   = hx::asys::libuv::context(ctx);
    auto pathBuffer = std::make_unique<hx::strbuf>();
    auto pathString = path.utf8_str(pathBuffer.get());

    libuvCtx->ctx->enqueue(std::move(std::make_unique<CreateWork>(cbSuccess, cbFailure, std::move(pathBuffer), pathString, permissions)));
}

void hx::asys::filesystem::Directory_obj::rename(Context ctx, String oldPath, String newPath, Dynamic cbSuccess, Dynamic cbFailure)
{
    class RenameRequest final : public hx::asys::libuv::filesystem::FsRequest
    {
        std::unique_ptr<hx::strbuf> oldPathBuffer;
        std::unique_ptr<hx::strbuf> newPathBuffer;

    public:
        RenameRequest(Dynamic _cbSuccess, Dynamic _cbFailure, std::unique_ptr<hx::strbuf> _oldPathBuffer, std::unique_ptr<hx::strbuf> _newPathBuffer)
            : FsRequest(_cbSuccess, _cbFailure)
            , oldPathBuffer(std::move(_oldPathBuffer))
            , newPathBuffer(std::move(_newPathBuffer)) {}
    };

    class RenameWork final : public hx::asys::libuv::WorkRequest
    {
        std::unique_ptr<hx::strbuf> oldPathBuffer;
        std::unique_ptr<hx::strbuf> newPathBuffer;
        const char* oldPath;
        const char* newPath;

    public:
        RenameWork(Dynamic _cbSuccess, Dynamic _cbFailure, std::unique_ptr<hx::strbuf> _oldPathBuffer, std::unique_ptr<hx::strbuf> _newPathBuffer, const char* _oldPath, const char* _newPath)
            : WorkRequest(_cbSuccess, _cbFailure)
            , newPathBuffer(std::move(_newPathBuffer))
            , oldPathBuffer(std::move(_oldPathBuffer))
            , newPath(_newPath)
            , oldPath(_oldPath) {}

        void run(uv_loop_t* loop) override
        {
            auto request = std::make_unique<RenameRequest>(cbSuccess.rooted, cbFailure.rooted, std::move(oldPathBuffer), std::move(newPathBuffer));
            auto result  = uv_fs_rename(loop, &request->uv, oldPath, newPath, hx::asys::libuv::filesystem::FsRequest::callback);
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

    auto libuvCtx      = hx::asys::libuv::context(ctx);
    auto oldPathBuffer = std::make_unique<hx::strbuf>();
    auto newPathBuffer = std::make_unique<hx::strbuf>();
    auto oldPathString = oldPath.utf8_str(oldPathBuffer.get());
    auto newPathString = newPath.utf8_str(newPathBuffer.get());

    libuvCtx->ctx->enqueue(std::move(std::make_unique<RenameWork>(cbSuccess, cbFailure, std::move(oldPathBuffer), std::move(newPathBuffer), oldPathString, newPathString)));
}

void hx::asys::filesystem::Directory_obj::check(Context ctx, String path, FileAccessMode accessMode, Dynamic cbSuccess, Dynamic cbFailure)
{
    class CheckRequest final : public hx::asys::libuv::filesystem::FsRequest
    {
        std::unique_ptr<hx::strbuf> pathBuffer;

    public:
        CheckRequest(Dynamic _cbSuccess, Dynamic _cbFailure, std::unique_ptr<hx::strbuf> _pathBuffer)
            : FsRequest(_cbSuccess, _cbFailure)
            , pathBuffer(std::move(_pathBuffer)) {}

        static void onCallback(uv_fs_t* request)
        {
            auto gcZone    = hx::AutoGCZone();
            auto spRequest = std::unique_ptr<hx::asys::libuv::filesystem::FsRequest>(static_cast<hx::asys::libuv::filesystem::FsRequest*>(request->data));

            if (spRequest->uv.result == UV_ENOENT || spRequest->uv.result == UV_EACCES)
            {
                Dynamic(spRequest->cbSuccess.rooted)(false);
            }
            else if (spRequest->uv.result < 0)
            {
                Dynamic(spRequest->cbFailure.rooted)(hx::asys::libuv::uv_err_to_enum(spRequest->uv.result));
            }
            else
            {
                Dynamic(spRequest->cbSuccess.rooted)(true);
            }
        }
    };

    class CheckWork final : public hx::asys::libuv::WorkRequest
    {
        std::unique_ptr<hx::strbuf> pathBuffer;
        const char* path;
        const int flags;

    public:
        CheckWork(Dynamic _cbSuccess, Dynamic _cbFailure, std::unique_ptr<hx::strbuf> _pathBuffer, const char* _path, const int _flags)
            : WorkRequest(_cbSuccess, _cbFailure)
            , pathBuffer(std::move(_pathBuffer))
            , path(_path)
            , flags(_flags) {}

        void run(uv_loop_t* loop) override
        {
            auto request = std::make_unique<CheckRequest>(cbSuccess.rooted, cbFailure.rooted, std::move(pathBuffer));
            auto result  = uv_fs_access(loop, &request->uv, path, flags, CheckRequest::onCallback);
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

    auto libuvCtx   = hx::asys::libuv::context(ctx);
    auto pathBuffer = std::make_unique<hx::strbuf>();
    auto pathString = path.utf8_str(pathBuffer.get());
    auto mode       = 0;

    if (static_cast<bool>(accessMode & hx::asys::filesystem::FileAccessMode::exists))
    {
        mode |= F_OK;
    }
    if (static_cast<bool>(accessMode & hx::asys::filesystem::FileAccessMode::executable))
    {
        mode |= X_OK;
    }
    if (static_cast<bool>(accessMode & hx::asys::filesystem::FileAccessMode::writable))
    {
        mode |= W_OK;
    }
    if (static_cast<bool>(accessMode & hx::asys::filesystem::FileAccessMode::readable))
    {
        mode |= R_OK;
    }

    libuvCtx->ctx->enqueue(std::move(std::make_unique<CheckWork>(cbSuccess, cbFailure, std::move(pathBuffer), pathString, mode)));
}

void hx::asys::filesystem::Directory_obj::deleteFile(Context ctx, String path, Dynamic cbSuccess, Dynamic cbFailure)
{
    class DeleteFileRequest final : public hx::asys::libuv::filesystem::FsRequest
    {
        std::unique_ptr<hx::strbuf> pathBuffer;

    public:
        DeleteFileRequest(Dynamic _cbSuccess, Dynamic _cbFailure, std::unique_ptr<hx::strbuf> _pathBuffer)
            : FsRequest(_cbSuccess, _cbFailure)
            , pathBuffer(std::move(_pathBuffer)) {
        }
    };

    class DeleteFileWork final : public hx::asys::libuv::WorkRequest
    {
        std::unique_ptr<hx::strbuf> pathBuffer;
        const char* path;

    public:
        DeleteFileWork(Dynamic _cbSuccess, Dynamic _cbFailure, std::unique_ptr<hx::strbuf> _pathBuffer, const char* _path)
            : WorkRequest(_cbSuccess, _cbFailure)
            , pathBuffer(std::move(_pathBuffer))
            , path(_path) {}

        void run(uv_loop_t* loop) override
        {
            auto request = std::make_unique<DeleteFileRequest>(cbSuccess.rooted, cbFailure.rooted, std::move(pathBuffer));
            auto result  = uv_fs_unlink(loop, &request->uv, path, hx::asys::libuv::filesystem::FsRequest::callback);
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

    auto libuvCtx   = hx::asys::libuv::context(ctx);
    auto pathBuffer = std::make_unique<hx::strbuf>();
    auto pathString = path.utf8_str(pathBuffer.get());

    libuvCtx->ctx->enqueue(std::move(std::make_unique<DeleteFileWork>(cbSuccess, cbFailure, std::move(pathBuffer), pathString)));
}

void hx::asys::filesystem::Directory_obj::deleteDirectory(Context ctx, String path, Dynamic cbSuccess, Dynamic cbFailure)
{
    class DeleteDirectoryRequest final : public hx::asys::libuv::filesystem::FsRequest
    {
        std::unique_ptr<hx::strbuf> pathBuffer;

    public:
        DeleteDirectoryRequest(Dynamic _cbSuccess, Dynamic _cbFailure, std::unique_ptr<hx::strbuf> _pathBuffer)
            : FsRequest(_cbSuccess, _cbFailure)
            , pathBuffer(std::move(_pathBuffer)) {
        }
    };

    class DeleteDirectoryWork final : public hx::asys::libuv::WorkRequest
    {
        std::unique_ptr<hx::strbuf> pathBuffer;
        const char* path;

    public:
        DeleteDirectoryWork(Dynamic _cbSuccess, Dynamic _cbFailure, std::unique_ptr<hx::strbuf> _pathBuffer, const char* _path)
            : WorkRequest(_cbSuccess, _cbFailure)
            , pathBuffer(std::move(_pathBuffer))
            , path(_path) {
        }

        void run(uv_loop_t* loop) override
        {
            auto request = std::make_unique<DeleteDirectoryRequest>(cbSuccess.rooted, cbFailure.rooted, std::move(pathBuffer));
            auto result  = uv_fs_rmdir(loop, &request->uv, path, hx::asys::libuv::filesystem::FsRequest::callback);
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

    auto libuvCtx   = hx::asys::libuv::context(ctx);
    auto pathBuffer = std::make_unique<hx::strbuf>();
    auto pathString = path.utf8_str(pathBuffer.get());

    libuvCtx->ctx->enqueue(std::move(std::make_unique<DeleteDirectoryWork>(cbSuccess, cbFailure, std::move(pathBuffer), pathString)));
}

void hx::asys::filesystem::Directory_obj::isDirectory(Context ctx, String path, Dynamic cbSuccess, Dynamic cbFailure)
{
    class Request final : public hx::asys::libuv::filesystem::FsRequest
    {
        std::unique_ptr<hx::strbuf> pathBuffer;

    public:
        Request(Dynamic _cbSuccess, Dynamic _cbFailure, std::unique_ptr<hx::strbuf> _pathBuffer)
            : FsRequest(_cbSuccess, _cbFailure)
            , pathBuffer(std::move(_pathBuffer)) {
        }
    };

    class Work final : public hx::asys::libuv::WorkRequest
    {
        std::unique_ptr<hx::strbuf> pathBuffer;
        const char* path;

    public:
        Work(Dynamic _cbSuccess, Dynamic _cbFailure, std::unique_ptr<hx::strbuf> _pathBuffer, const char* _path)
            : WorkRequest(_cbSuccess, _cbFailure)
            , pathBuffer(std::move(_pathBuffer))
            , path(_path) {
        }

        void run(uv_loop_t* loop) override
        {
            auto request = std::make_unique<Request>(cbSuccess.rooted, cbFailure.rooted, std::move(pathBuffer));
            auto wrapper = [](uv_fs_t* request) { check_type_callback(S_IFDIR, request); };
            auto result  = uv_fs_stat(loop , &request->uv, path, wrapper);
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

    auto libuvCtx   = hx::asys::libuv::context(ctx);
    auto pathBuffer = std::make_unique<hx::strbuf>();
    auto pathString = path.utf8_str(pathBuffer.get());

    libuvCtx->ctx->enqueue(std::move(std::make_unique<Work>(cbSuccess, cbFailure, std::move(pathBuffer), pathString)));
}

void hx::asys::filesystem::Directory_obj::isFile(Context ctx, String path, Dynamic cbSuccess, Dynamic cbFailure)
{
    class Request final : public hx::asys::libuv::filesystem::FsRequest
    {
        std::unique_ptr<hx::strbuf> pathBuffer;

    public:
        Request(Dynamic _cbSuccess, Dynamic _cbFailure, std::unique_ptr<hx::strbuf> _pathBuffer)
            : FsRequest(_cbSuccess, _cbFailure)
            , pathBuffer(std::move(_pathBuffer)) {
        }
    };

    class Work final : public hx::asys::libuv::WorkRequest
    {
        std::unique_ptr<hx::strbuf> pathBuffer;
        const char* path;

    public:
        Work(Dynamic _cbSuccess, Dynamic _cbFailure, std::unique_ptr<hx::strbuf> _pathBuffer, const char* _path)
            : WorkRequest(_cbSuccess, _cbFailure)
            , pathBuffer(std::move(_pathBuffer))
            , path(_path) {
        }

        void run(uv_loop_t* loop) override
        {
            auto request = std::make_unique<Request>(cbSuccess.rooted, cbFailure.rooted, std::move(pathBuffer));
            auto wrapper = [](uv_fs_t* request) { check_type_callback(S_IFREG, request); };
            auto result  = uv_fs_stat(loop, &request->uv, path, wrapper);
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

    auto libuvCtx   = hx::asys::libuv::context(ctx);
    auto pathBuffer = std::make_unique<hx::strbuf>();
    auto pathString = path.utf8_str(pathBuffer.get());

    libuvCtx->ctx->enqueue(std::move(std::make_unique<Work>(cbSuccess, cbFailure, std::move(pathBuffer), pathString)));
}

void hx::asys::filesystem::Directory_obj::isLink(Context ctx, String path, Dynamic cbSuccess, Dynamic cbFailure)
{
    class Request final : public hx::asys::libuv::filesystem::FsRequest
    {
        std::unique_ptr<hx::strbuf> pathBuffer;

    public:
        Request(Dynamic _cbSuccess, Dynamic _cbFailure, std::unique_ptr<hx::strbuf> _pathBuffer)
            : FsRequest(_cbSuccess, _cbFailure)
            , pathBuffer(std::move(_pathBuffer)) {
        }
    };

    class Work final : public hx::asys::libuv::WorkRequest
    {
        std::unique_ptr<hx::strbuf> pathBuffer;
        const char* path;

    public:
        Work(Dynamic _cbSuccess, Dynamic _cbFailure, std::unique_ptr<hx::strbuf> _pathBuffer, const char* _path)
            : WorkRequest(_cbSuccess, _cbFailure)
            , pathBuffer(std::move(_pathBuffer))
            , path(_path) {
        }

        void run(uv_loop_t* loop) override
        {
            auto request = std::make_unique<Request>(cbSuccess.rooted, cbFailure.rooted, std::move(pathBuffer));
            auto wrapper = [](uv_fs_t* request) { check_type_callback(S_IFLNK, request); };
            auto result  = uv_fs_stat(loop, &request->uv, path, wrapper);
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

    auto libuvCtx   = hx::asys::libuv::context(ctx);
    auto pathBuffer = std::make_unique<hx::strbuf>();
    auto pathString = path.utf8_str(pathBuffer.get());

    libuvCtx->ctx->enqueue(std::move(std::make_unique<Work>(cbSuccess, cbFailure, std::move(pathBuffer), pathString)));
}

void hx::asys::filesystem::Directory_obj::setLinkOwner(Context ctx, String path, int user, int group, Dynamic cbSuccess, Dynamic cbFailure)
{
    class SetLinkOwnerRequest final : public hx::asys::libuv::filesystem::FsRequest
    {
        std::unique_ptr<hx::strbuf> pathBuffer;

    public:
        SetLinkOwnerRequest(Dynamic _cbSuccess, Dynamic _cbFailure, std::unique_ptr<hx::strbuf> _pathBuffer)
            : FsRequest(_cbSuccess, _cbFailure)
            , pathBuffer(std::move(_pathBuffer)) {}
    };

    class SetLinkOwnerWork final : public hx::asys::libuv::WorkRequest
    {
        std::unique_ptr<hx::strbuf> pathBuffer;
        const char* path;
        const int user;
        const int group;

    public:
        SetLinkOwnerWork(Dynamic _cbSuccess, Dynamic _cbFailure, std::unique_ptr<hx::strbuf> _pathBuffer, const char* _path, const int _user, const int _group)
            : WorkRequest(_cbSuccess, _cbFailure)
            , pathBuffer(std::move(_pathBuffer))
            , path(_path)
            , user(_user)
            , group(_group) {}

        void run(uv_loop_t* loop) override
        {
            auto request = std::make_unique<SetLinkOwnerRequest>(cbSuccess.rooted, cbFailure.rooted, std::move(pathBuffer));
            auto result  = uv_fs_lchown(loop, &request->uv, path, user, group, hx::asys::libuv::filesystem::FsRequest::callback);
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

    auto libuvCtx   = hx::asys::libuv::context(ctx);
    auto pathBuffer = std::make_unique<hx::strbuf>();
    auto pathString = path.utf8_str(pathBuffer.get());

    libuvCtx->ctx->enqueue(std::move(std::make_unique<SetLinkOwnerWork>(cbSuccess, cbFailure, std::move(pathBuffer), pathString, user, group)));
}

void hx::asys::filesystem::Directory_obj::link(Context ctx, String target, String path, int type, Dynamic cbSuccess, Dynamic cbFailure)
{
    class LinkRequest final : public hx::asys::libuv::filesystem::FsRequest
    {
        std::unique_ptr<hx::strbuf> targetBuffer;
        std::unique_ptr<hx::strbuf> pathBuffer;

    public:
        LinkRequest(Dynamic _cbSuccess, Dynamic _cbFailure, std::unique_ptr<hx::strbuf> _targetBuffer, std::unique_ptr<hx::strbuf> _pathBuffer)
            : FsRequest(_cbSuccess, _cbFailure)
            , targetBuffer(std::move(_targetBuffer))
            , pathBuffer(std::move(_pathBuffer)) {}
    };

    class LinkWork final : public hx::asys::libuv::WorkRequest
    {
        std::unique_ptr<hx::strbuf> targetBuffer;
        std::unique_ptr<hx::strbuf> pathBuffer;
        const char* target;
        const char* path;
        const int type;

    public:
        LinkWork(Dynamic _cbSuccess, Dynamic _cbFailure, std::unique_ptr<hx::strbuf> _targetBuffer, std::unique_ptr<hx::strbuf> _pathBuffer, const char* _target, const char* _path, const int _type)
            : WorkRequest(_cbSuccess, _cbFailure)
            , targetBuffer(std::move(_targetBuffer))
            , pathBuffer(std::move(_pathBuffer))
            , target(_target)
            , path(_path)
            , type(_type) {}

        void run(uv_loop_t* loop) override
        {
            auto request = std::make_unique<LinkRequest>(cbSuccess.rooted, cbFailure.rooted, std::move(targetBuffer), std::move(pathBuffer));
            auto result  = type == 0
                ? uv_fs_link(loop, &request->uv, target, path, hx::asys::libuv::filesystem::FsRequest::callback)
                : uv_fs_symlink(loop, &request->uv, target, path, 0, hx::asys::libuv::filesystem::FsRequest::callback);
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

    auto libuvCtx     = hx::asys::libuv::context(ctx);
    auto targetBuffer = std::make_unique<hx::strbuf>();
    auto pathBuffer   = std::make_unique<hx::strbuf>();
    auto targetString = path.utf8_str(targetBuffer.get());
    auto pathString   = path.utf8_str(pathBuffer.get());

    libuvCtx->ctx->enqueue(std::move(std::make_unique<LinkWork>(cbSuccess, cbFailure, std::move(targetBuffer), std::move(pathBuffer), targetString, pathString, type)));
}

void hx::asys::filesystem::Directory_obj::linkInfo(Context ctx, String path, Dynamic cbSuccess, Dynamic cbFailure)
{
    class LinkInfoRequest final : public hx::asys::libuv::filesystem::FsRequest
    {
        std::unique_ptr<hx::strbuf> pathBuffer;

    public:
        LinkInfoRequest(Dynamic _cbSuccess, Dynamic _cbFailure, std::unique_ptr<hx::strbuf> _pathBuffer)
            : FsRequest(_cbSuccess, _cbFailure)
            , pathBuffer(std::move(_pathBuffer)) {
        }

        static void onCallback(uv_fs_t* request)
        {
            auto spRequest = std::unique_ptr<hx::asys::libuv::filesystem::FsRequest>(static_cast<hx::asys::libuv::filesystem::FsRequest*>(request->data));
            auto gcZone    = hx::AutoGCZone();

            if (spRequest->uv.result < 0)
            {
                Dynamic(spRequest->cbFailure.rooted)(hx::asys::libuv::uv_err_to_enum(spRequest->uv.result));
            }
            else
            {
                auto statBuf = hx::Anon_obj::Create(13);
                statBuf->setFixed(0, HX_CSTRING("atime"), static_cast<int>(spRequest->uv.statbuf.st_atim.tv_sec));
                statBuf->setFixed(1, HX_CSTRING("mtime"), static_cast<int>(spRequest->uv.statbuf.st_mtim.tv_sec));
                statBuf->setFixed(2, HX_CSTRING("ctime"), static_cast<int>(spRequest->uv.statbuf.st_ctim.tv_sec));
                statBuf->setFixed(3, HX_CSTRING("dev"), static_cast<int>(spRequest->uv.statbuf.st_dev));
                statBuf->setFixed(4, HX_CSTRING("uid"), static_cast<int>(spRequest->uv.statbuf.st_uid));
                statBuf->setFixed(5, HX_CSTRING("gid"), static_cast<int>(spRequest->uv.statbuf.st_gid));
                statBuf->setFixed(6, HX_CSTRING("ino"), static_cast<int>(spRequest->uv.statbuf.st_ino));
                statBuf->setFixed(7, HX_CSTRING("mode"), static_cast<int>(spRequest->uv.statbuf.st_mode));
                statBuf->setFixed(8, HX_CSTRING("nlink"), static_cast<int>(spRequest->uv.statbuf.st_nlink));
                statBuf->setFixed(9, HX_CSTRING("rdev"), static_cast<int>(spRequest->uv.statbuf.st_rdev));
                statBuf->setFixed(10, HX_CSTRING("size"), static_cast<int>(spRequest->uv.statbuf.st_size));
                statBuf->setFixed(11, HX_CSTRING("blksize"), static_cast<int>(spRequest->uv.statbuf.st_blksize));
                statBuf->setFixed(12, HX_CSTRING("blocks"), static_cast<int>(spRequest->uv.statbuf.st_blocks));

                Dynamic(spRequest->cbSuccess.rooted)(statBuf);
            }
        }
    };

    class LinkInfoWork final : public hx::asys::libuv::WorkRequest
    {
        std::unique_ptr<hx::strbuf> pathBuffer;
        const char* path;

    public:
        LinkInfoWork(Dynamic _cbSuccess, Dynamic _cbFailure, std::unique_ptr<hx::strbuf> _pathBuffer, const char* _path)
            : WorkRequest(_cbSuccess, _cbFailure)
            , pathBuffer(std::move(_pathBuffer))
            , path(_path) {
        }

        void run(uv_loop_t* loop) override
        {
            auto request = std::make_unique<LinkInfoRequest>(cbSuccess.rooted, cbFailure.rooted, std::move(pathBuffer));
            auto result  = uv_fs_lstat(loop, &request->uv, path, LinkInfoRequest::onCallback);
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

    auto libuvCtx   = hx::asys::libuv::context(ctx);
    auto pathBuffer = std::make_unique<hx::strbuf>();
    auto pathString = path.utf8_str(pathBuffer.get());

    libuvCtx->ctx->enqueue(std::move(std::make_unique<LinkInfoWork>(cbSuccess, cbFailure, std::move(pathBuffer), pathString)));
}

void hx::asys::filesystem::Directory_obj::readLink(Context ctx, String path, Dynamic cbSuccess, Dynamic cbFailure)
{
    class ReadLinkRequest final : public hx::asys::libuv::filesystem::FsRequest
    {
        std::unique_ptr<hx::strbuf> pathBuffer;

    public:
        ReadLinkRequest(Dynamic _cbSuccess, Dynamic _cbFailure, std::unique_ptr<hx::strbuf> _pathBuffer)
            : FsRequest(_cbSuccess, _cbFailure)
            , pathBuffer(std::move(_pathBuffer)) {
        }
    };

    class ReadLinkWork final : public hx::asys::libuv::WorkRequest
    {
        std::unique_ptr<hx::strbuf> pathBuffer;
        const char* path;

    public:
        ReadLinkWork(Dynamic _cbSuccess, Dynamic _cbFailure, std::unique_ptr<hx::strbuf> _pathBuffer, const char* _path)
            : WorkRequest(_cbSuccess, _cbFailure)
            , pathBuffer(std::move(_pathBuffer))
            , path(_path) {
        }

        void run(uv_loop_t* loop) override
        {
            auto request = std::make_unique<ReadLinkRequest>(cbSuccess.rooted, cbFailure.rooted, std::move(pathBuffer));
            auto result  = uv_fs_readlink(loop, &request->uv, request->path, path_callback);
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

    auto libuvCtx = hx::asys::libuv::context(ctx);
    auto pathBuffer = std::make_unique<hx::strbuf>();
    auto pathString = path.utf8_str(pathBuffer.get());

    libuvCtx->ctx->enqueue(std::move(std::make_unique<ReadLinkWork>(cbSuccess, cbFailure, std::move(pathBuffer), pathString)));
}

void hx::asys::filesystem::Directory_obj::copyFile(Context ctx, String source, String destination, bool overwrite, Dynamic cbSuccess, Dynamic cbFailure)
{
    class CopyFileRequest final : public hx::asys::libuv::filesystem::FsRequest
    {
        std::unique_ptr<hx::strbuf> srcPathBuffer;
        std::unique_ptr<hx::strbuf> dstPathBuffer;

    public:
        CopyFileRequest(Dynamic _cbSuccess, Dynamic _cbFailure, std::unique_ptr<hx::strbuf> _srcPathBuffer, std::unique_ptr<hx::strbuf> _dstPathBuffer)
            : FsRequest(_cbSuccess, _cbFailure)
            , srcPathBuffer(std::move(_srcPathBuffer))
            , dstPathBuffer(std::move(_dstPathBuffer)) {}
    };

    class CopyFileWork final : public hx::asys::libuv::WorkRequest
    {
        std::unique_ptr<hx::strbuf> srcPathBuffer;
        std::unique_ptr<hx::strbuf> dstPathBuffer;
        const char* srcPath;
        const char* dstPath;

    public:
        CopyFileWork(Dynamic _cbSuccess, Dynamic _cbFailure, std::unique_ptr<hx::strbuf> _srcPathBuffer, std::unique_ptr<hx::strbuf> _dstPathBuffer, const char* _srcPath, const char* _dstPath)
            : WorkRequest(_cbSuccess, _cbFailure)
            , srcPathBuffer(std::move(_srcPathBuffer))
            , dstPathBuffer(std::move(_dstPathBuffer))
            , srcPath(_srcPath)
            , dstPath(_dstPath) {}

        void run(uv_loop_t* loop) override
        {
            auto request = std::make_unique<CopyFileRequest>(cbSuccess.rooted, cbFailure.rooted, std::move(srcPathBuffer), std::move(dstPathBuffer));
            auto result  = uv_fs_rename(loop, &request->uv, srcPath, dstPath, hx::asys::libuv::filesystem::FsRequest::callback);
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

    auto libuvCtx      = hx::asys::libuv::context(ctx);
    auto srcPathBuffer = std::make_unique<hx::strbuf>();
    auto dstPathBuffer = std::make_unique<hx::strbuf>();
    auto srcPathString = source.utf8_str(srcPathBuffer.get());
    auto dstPathString = destination.utf8_str(dstPathBuffer.get());

    libuvCtx->ctx->enqueue(std::move(std::make_unique<CopyFileWork>(cbSuccess, cbFailure, std::move(srcPathBuffer), std::move(dstPathBuffer), srcPathString, dstPathString)));
}

void hx::asys::filesystem::Directory_obj::realPath(Context ctx, String path, Dynamic cbSuccess, Dynamic cbFailure)
{
    class RealPathRequest final : public hx::asys::libuv::filesystem::FsRequest
    {
        std::unique_ptr<hx::strbuf> pathBuffer;

    public:
        RealPathRequest(Dynamic _cbSuccess, Dynamic _cbFailure, std::unique_ptr<hx::strbuf> _pathBuffer)
            : FsRequest(_cbSuccess, _cbFailure)
            , pathBuffer(std::move(_pathBuffer)) {}
    };

    class RealPathWork final : public hx::asys::libuv::WorkRequest
    {
        std::unique_ptr<hx::strbuf> pathBuffer;
        const char* path;

    public:
        RealPathWork(Dynamic _cbSuccess, Dynamic _cbFailure, std::unique_ptr<hx::strbuf> _pathBuffer, const char* _path)
            : WorkRequest(_cbSuccess, _cbFailure)
            , pathBuffer(std::move(_pathBuffer))
            , path(_path) {}

        void run(uv_loop_t* loop) override
        {
            auto request = std::make_unique<RealPathRequest>(cbSuccess.rooted, cbFailure.rooted, std::move(pathBuffer));
            auto result  = uv_fs_realpath(loop, &request->uv, request->path, path_callback);
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

    auto libuvCtx   = hx::asys::libuv::context(ctx);
    auto pathBuffer = std::make_unique<hx::strbuf>();
    auto pathString = path.utf8_str(pathBuffer.get());

    libuvCtx->ctx->enqueue(std::move(std::make_unique<RealPathWork>(cbSuccess, cbFailure, std::move(pathBuffer), pathString)));
}