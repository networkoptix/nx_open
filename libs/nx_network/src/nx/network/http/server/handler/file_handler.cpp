// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "file_handler.h"

#include <nx/network/socket_global.h>
#include <nx/network/url/url_parse_helper.h>
#include <nx/utils/fs/async_file_processor.h>
#include <nx/utils/log/log.h>
#include <nx/utils/string.h>

#include "../../file_body.h"
#include "../../global_context.h"

namespace nx::network::http::server::handler {

FileDownloader::FileDownloader(
    const std::string& requestPathPrefix,
    const std::string& filePathPrefix,
    std::size_t fileReadSize)
    :
    m_requestPathPrefix(requestPathPrefix),
    m_filePathPrefix(filePathPrefix),
    m_fileAsyncIoScheduler(
        &nx::network::SocketGlobals::instance().httpGlobalContext().fileAsyncIoScheduler),
    m_fileReadSize(fileReadSize)
{
}

void FileDownloader::processRequest(
    RequestContext requestContext,
    RequestProcessedHandler completionHandler)
{
    if (requestContext.request.requestLine.method != Method::get)
        return completionHandler(StatusCode::notAllowed);

    const auto [statusCode, filePath] = composeFilePath(
        requestContext.request.requestLine.url.path().toStdString());
    if (!StatusCode::isSuccessCode(statusCode))
    {
        NX_DEBUG(this, "Failed to build file path. "
            "Request path %1, requestPathPrefix %2, filePathPrefix %3",
            requestContext.request.requestLine.url.path(), m_requestPathPrefix, m_filePathPrefix);
        return completionHandler(statusCode);
    }

    NX_VERBOSE(this, "Request %1 translated to file %2",
        requestContext.request.requestLine.url.path(), filePath);

    m_filePath = filePath;
    m_completionHandler = std::move(completionHandler);

    m_fileAsyncIoScheduler->stat(
        m_filePath,
        [this](auto&&... args) { onStatCompletion(std::forward<decltype(args)>(args)...); });
}

std::tuple<StatusCode::Value, std::string> FileDownloader::composeFilePath(
    const std::string_view& requestPath)
{
    if (!nx::utils::startsWith(requestPath, m_requestPathPrefix))
        return std::make_tuple(StatusCode::internalServerError, std::string());

    const auto relativeFilePath =
        url::normalizePath(requestPath.substr(m_requestPathPrefix.size()));

    if (nx::utils::startsWith(relativeFilePath, ".."))
    {
        NX_DEBUG(this, "Rejecting invalid path %1. Accessing parent dirs is forbidden");
        return std::make_tuple(StatusCode::forbidden, std::string());
    }

    return std::make_tuple(
        StatusCode::ok,
        url::normalizePath(nx::utils::buildString(m_filePathPrefix, "/", relativeFilePath)));
}

void FileDownloader::onStatCompletion(
    SystemError::ErrorCode resultCode,
    nx::utils::fs::FileStat fileStat)
{
    NX_VERBOSE(this, "File %1 stat completed with result %2",
        m_filePath, SystemError::toString(resultCode));

    if (resultCode != SystemError::noError)
        return m_completionHandler(systemErrorToStatus(resultCode));

    m_fileStat = fileStat;
    m_file = std::make_unique<nx::utils::fs::File>(m_filePath);
    m_fileAsyncIoScheduler->open(
        m_file.get(), QIODevice::ReadOnly, 0,
        [this](auto&&... args) { onOpenFileCompletion(std::forward<decltype(args)>(args)...); });
}

void FileDownloader::onOpenFileCompletion(SystemError::ErrorCode resultCode)
{
    NX_VERBOSE(this, "Open file %1 completed with result %2",
        m_filePath, SystemError::toString(resultCode));

    if (resultCode != SystemError::noError)
        return m_completionHandler(systemErrorToStatus(resultCode));

    auto body = std::make_unique<FileBody>(
        m_fileAsyncIoScheduler,
        m_filePath,
        std::exchange(m_file, nullptr),
        m_fileStat);
    body->setReadSize(m_fileReadSize);

    RequestResult result(StatusCode::ok);
    result.body = std::move(body);
    m_completionHandler(std::move(result));
}

StatusCode::Value FileDownloader::systemErrorToStatus(
    SystemError::ErrorCode systemError)
{
    if (systemError == SystemError::noError)
        return StatusCode::ok;
    else if (systemError == SystemError::fileNotFound || systemError == SystemError::pathNotFound)
        return StatusCode::notFound;
    else if (systemError == SystemError::noPermission)
        return StatusCode::forbidden;
    else
        return StatusCode::internalServerError;
}

} // namespace nx::network::http::server::handler
