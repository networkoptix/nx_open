// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <sys/stat.h>

#include <string>
#include <thread>

#include <QtCore/QIODevice>

#include <nx/utils/buffer.h>
#include <nx/utils/move_only_func.h>
#include <nx/utils/system_error.h>
#include <nx/utils/thread/sync_queue.h>

class IQnFile;

namespace nx::utils::fs {

#if defined(_WIN32)
    using FileStat = struct ::_stat64;
#elif defined(__APPLE__)
    using FileStat = struct stat;
#else
    using FileStat = struct stat64;
#endif

using StatHandler =
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode, FileStat fileStat)>;

using OpenHandler =
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)>;

using CloseHandler =
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode)>;

using ReadAllHandler =
    nx::utils::MoveOnlyFunc<void(SystemError::ErrorCode, nx::Buffer)>;

using IoCompletionHandler = nx::utils::MoveOnlyFunc<
    void(SystemError::ErrorCode /*errorCode*/, std::size_t /*bytesTransferred*/)>;

class AbstractTask;

/**
 * Implements asynchronous file operations.
 * NOTE: This class introduced as a quick solution for handler::FileDownloader.
 * It is not intended to be a right/high-performance async file I/O.
 */
class NX_UTILS_API FileAsyncIoScheduler
{
public:
    FileAsyncIoScheduler();
    virtual ~FileAsyncIoScheduler();

    void stat(const std::string& path, StatHandler handler);

    void open(
        IQnFile* file,
        const QIODevice::OpenMode& mode,
        unsigned int systemDependentFlags,
        OpenHandler handler);

    /**
     * Asynchronously reads opened file.
     */
    void read(
        IQnFile* file,
        nx::Buffer* buf,
        IoCompletionHandler handler);

    /**
     * Asynchronously reads opened file to the end.
     */
    void readAll(IQnFile* file, ReadAllHandler handler);

    void write(
        IQnFile* file,
        const nx::Buffer& buffer,
        IoCompletionHandler handler);

    void close(IQnFile* file, CloseHandler handler);

private:
    void taskProcessingThread();

private:
    std::thread m_ioThread;
    nx::utils::SyncQueue<std::unique_ptr<AbstractTask>> m_tasks;
};

} // namespace nx::utils::fs
