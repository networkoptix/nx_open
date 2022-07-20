// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <string>
#include <tuple>

#include <nx/utils/fs/file.h>

#include "../../file_body.h"
#include "../abstract_http_request_handler.h"

namespace nx::utils::fs { class FileAsyncIoScheduler; }

namespace nx::network::http::server::handler {

/**
 * Sends file from local file system over HTTP. Supports only GET method.
 * Path to file to return is calculated by removing requestPathPrefix from the request path
 * and appending the remainder to filePathPrefix.
 * NOTE: Providing files from parent directory is forbidden.
 */
class NX_NETWORK_API FileDownloader:
    public RequestHandlerWithContext
{
public:
    FileDownloader(
        const std::string& requestPathPrefix,
        const std::string& filePathPrefix,
        std::size_t fileReadSize = FileBody::kDefaultReadSize);

    virtual void processRequest(
        RequestContext requestContext,
        RequestProcessedHandler completionHandler) override;

protected:
    std::tuple<StatusCode::Value, std::string> composeFilePath(
        const std::string_view& requestPath);

private:
    const std::string m_requestPathPrefix;
    const std::string m_filePathPrefix;
    nx::utils::fs::FileAsyncIoScheduler* m_fileAsyncIoScheduler = nullptr;
    std::string m_filePath;
    nx::utils::fs::FileStat m_fileStat;
    std::unique_ptr<nx::utils::fs::File> m_file;
    RequestProcessedHandler m_completionHandler;
    std::size_t m_fileReadSize = FileBody::kDefaultReadSize;

    void onStatCompletion(
        SystemError::ErrorCode resultCode,
        nx::utils::fs::FileStat fileStat);

    void onOpenFileCompletion(SystemError::ErrorCode resultCode);

    StatusCode::Value systemErrorToStatus(SystemError::ErrorCode systemError);
};

} // namespace nx::network::http::server::handler
