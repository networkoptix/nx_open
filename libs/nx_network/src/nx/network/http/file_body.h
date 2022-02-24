// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/utils/async_operation_guard.h>
#include <nx/utils/fs/file.h>
#include <nx/utils/std/filesystem.h>

#include "abstract_msg_body_source.h"

namespace nx::utils::fs { class FileAsyncIoScheduler; }

namespace nx::network::http {

/**
 * Streams opened file as the message body.
 * It is not recommended to use this class directly.
 * Consider using nx::network::http::server::handler::FileDownloader instead.
 */
class FileBody:
    public AbstractMsgBodySource
{
    using base_type = AbstractMsgBodySource;

public:
    static constexpr std::size_t kDefaultReadSize = 64 * 1024;

    FileBody(
        nx::utils::fs::FileAsyncIoScheduler* fileAsyncIoScheduler,
        const std::string& filePath,
        std::unique_ptr<nx::utils::fs::File> file,
        nx::utils::fs::FileStat fileStat);

    virtual std::string mimeType() const override;
    virtual std::optional<uint64_t> contentLength() const override;
    virtual void readAsync(CompletionHandler completionHandler) override;

    /**
     * By default, the read size is kDefaultReadSize.
     */
    void setReadSize(std::size_t readSize);

private:
    virtual void stopWhileInAioThread() override;

private:
    nx::utils::fs::FileAsyncIoScheduler* m_fileAsyncIoScheduler = nullptr;
    const std::string m_filePath;
    std::unique_ptr<nx::utils::fs::File> m_file;
    nx::utils::fs::FileStat m_fileStat;
    bool m_done = false;
    nx::utils::AsyncOperationGuard m_guard;
    std::size_t m_readSize = kDefaultReadSize;
    nx::Buffer m_readBuf;
};

} // namespace nx::network::http
