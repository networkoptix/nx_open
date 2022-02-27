// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "file_body.h"

#include <nx/network/socket_global.h>

#include "global_context.h"

namespace nx::network::http {

FileBody::FileBody(
    nx::utils::fs::FileAsyncIoScheduler* fileAsyncIoScheduler,
    const std::string& filePath,
    std::unique_ptr<nx::utils::fs::File> file,
    nx::utils::fs::FileStat fileStat)
    :
    m_fileAsyncIoScheduler(fileAsyncIoScheduler),
    m_filePath(filePath),
    m_file(std::move(file)),
    m_fileStat(fileStat)
{
}

std::string FileBody::mimeType() const
{
    const auto extension = nx::utils::filesystem::path(m_filePath).extension();

    if (extension == ".json")
        return "application/json";
    else if (extension == ".html" || extension == ".htm")
        return "text/html";
    else if (extension == ".jpeg" || extension == ".jpg")
        return "image/jpeg";
    else if (extension == ".js")
        return "text/javascript";
    else if (extension == ".txt")
        return "text/plain";
    else if (extension == ".xml")
        return "text/xml";
    else
        return "application/octet-stream";
}

std::optional<uint64_t> FileBody::contentLength() const
{
    return m_fileStat.st_size;
}

void FileBody::readAsync(CompletionHandler completionHandler)
{
    if (m_done)
    {
        // Reporting end of file.
        return post(
            [completionHandler = std::move(completionHandler)]()
            {
                completionHandler(SystemError::noError, nx::Buffer());
            });
    }

    m_readBuf.reserve(m_readBuf.size() + m_readSize);

    m_file->readAsync(
        m_fileAsyncIoScheduler,
        &m_readBuf,
        [this, completionHandler = std::move(completionHandler),
            guard = m_guard.sharedGuard()](
                SystemError::ErrorCode resultCode, std::size_t bytesRead) mutable
        {
            auto lock = guard->lock();
            if (!lock)
                return;

            dispatch(
                [this, completionHandler = std::move(completionHandler), resultCode, bytesRead]() mutable
                {
                    m_done = resultCode == SystemError::noError && bytesRead == 0;
                    completionHandler(resultCode, std::exchange(m_readBuf, nx::Buffer()));
                });
        });
}

void FileBody::setReadSize(std::size_t readSize)
{
    m_readSize = readSize;
}

void FileBody::stopWhileInAioThread()
{
    base_type::stopWhileInAioThread();

    m_guard.reset();
}

} // namespace nx::network::http
