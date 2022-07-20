// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "http_resource_storage.h"

#include <nx/network/http/server/http_message_dispatcher.h>
#include <nx/network/http/buffer_source.h>

namespace nx::network::http::test {

void ResourceStorage::save(const std::string& path, nx::Buffer content)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_pathToContent[path] = std::move(content);
}

std::optional<nx::Buffer> ResourceStorage::get(const std::string& path) const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    if (auto it = m_pathToContent.find(path); it != m_pathToContent.end())
        return it->second;
    return std::nullopt;
}

bool ResourceStorage::erase(const std::string& path)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_pathToContent.erase(path) > 0;
}

void ResourceStorage::registerHttpHandlers(
    AbstractMessageDispatcher* dispatcher,
    const std::string& basePath)
{
    setBasePath(basePath);
    // TODO: #akolesnikov Use m_basePath.

    dispatcher->registerRequestProcessorFunc(
        Method::post,
        kAnyPath,
        [this](auto&&... args) { postResource(std::forward<decltype(args)>(args)...); });

    dispatcher->registerRequestProcessorFunc(
        Method::put,
        kAnyPath,
        [this](auto&&... args) { postResource(std::forward<decltype(args)>(args)...); });

    dispatcher->registerRequestProcessorFunc(
        Method::get,
        kAnyPath,
        [this](auto&&... args) { getResource(std::forward<decltype(args)>(args)...); });

    dispatcher->registerRequestProcessorFunc(
        Method::delete_,
        kAnyPath,
        [this](auto&&... args) { deleteResource(std::forward<decltype(args)>(args)...); });
}

void ResourceStorage::setBasePath(const std::string& basePath)
{
    m_basePath = basePath;
}

void ResourceStorage::postResource(
    RequestContext requestContext,
    RequestProcessedHandler handler)
{
    save(
        requestContext.request.requestLine.url.path().toStdString(),
        std::move(requestContext.request.messageBody));

    handler(StatusCode::ok);
}

void ResourceStorage::getResource(
    RequestContext requestContext,
    RequestProcessedHandler handler)
{
    auto resource = get(
        requestContext.request.requestLine.url.path().toStdString());
    if (!resource)
        return handler(StatusCode::notFound);

    RequestResult result(StatusCode::ok);
    result.body = std::make_unique<BufferSource>("text/plain", std::move(*resource));

    handler(std::move(result));
}

void ResourceStorage::deleteResource(
    RequestContext requestContext,
    RequestProcessedHandler handler)
{
    if (!erase(requestContext.request.requestLine.url.path().toStdString()))
        return handler(StatusCode::notFound);

    handler(StatusCode::ok);
}

} // namespace nx::network::http::test
