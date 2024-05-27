// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "handler_pool.h"

#include <nx/network/url/url_parse_helper.h>

namespace nx::network::rest {

const nx::network::http::Method HandlerPool::kAnyHttpMethod = "";
const QString HandlerPool::kAnyPath = QString();

void HandlerPool::registerHandler(
    const QString& path,
    GlobalPermission readPermissions,
    GlobalPermission modifyPermissions,
    std::unique_ptr<Handler> handler,
    const nx::network::http::Method& method)
{
    handler->setPath(path);
    handler->setReadPermissions(readPermissions);
    handler->setModifyPermissions(modifyPermissions);
    if (m_crudSchemas)
        handler->setSchemas(m_crudSchemas);
    if (m_auditManager)
        handler->setAuditManager(m_auditManager);
    m_crudHandlers[method].addHandler(path, std::move(handler));
}

void HandlerPool::registerHandler(
    const QString& path,
    GlobalPermission permissions,
    std::unique_ptr<Handler> handler,
    const nx::network::http::Method& method)
{
    registerHandler(path, permissions, permissions, std::move(handler), method);
}

void HandlerPool::registerHandler(
    const QString& path,
    Handler* handler,
    GlobalPermission permissions)
{
    registerHandler(kAnyHttpMethod, path, handler, permissions);
}

void HandlerPool::registerHandler(
    const nx::network::http::Method& httpMethod,
    const QString& path,
    Handler* handler,
    GlobalPermission permissions)
{
    handler->setPath(path);
    handler->setReadPermissions(permissions);
    handler->setModifyPermissions(permissions);
    m_handlers[httpMethod][path].reset(handler);
}

Handler* HandlerPool::findHandlerOrThrow(Request* request, const QString& pathIgnorePrefix) const
{
    if (auto handler = findCrudHandlerOrThrow(request, pathIgnorePrefix))
        return handler;

    return findHandler(
        request->method(), nx::network::url::normalizedPath(request->path(), pathIgnorePrefix));
}

Handler* HandlerPool::findCrudHandlerOrThrow(
    Request* request, const QString& pathIgnorePrefix) const
{
    const auto& method = request->method();
    if (auto it = m_crudHandlers.find(method); it != m_crudHandlers.end())
    {
        if (auto handler = it->second.findHandlerOrThrow(request, pathIgnorePrefix))
            return handler;
    }
    if (auto it = m_crudHandlers.find(kAnyHttpMethod); it != m_crudHandlers.end())
    {
        if (auto handler = it->second.findHandlerOrThrow(request, pathIgnorePrefix))
            return handler;
    }
    return nullptr;
}

void HandlerPool::registerRedirectRule(const QString& path, const QString& newPath)
{
    m_redirectRules.insert(path, newPath);
}

std::optional<QString> HandlerPool::getRedirectRule(const QString& path)
{
    const auto it = m_redirectRules.find(path);
    if (it != m_redirectRules.end())
        return it.value();
    else
        return std::nullopt;
}

void HandlerPool::setSchemas(std::shared_ptr<json::OpenApiSchemas> schemas)
{
    m_crudSchemas = std::move(schemas);
}

Handler* HandlerPool::findHandler(
    const nx::network::http::Method& httpMethod, const QString& normalizedPath) const
{
    if (auto handler = findHandlerForSpecificMethod(httpMethod, normalizedPath))
        return handler;
    if (auto handler = findHandlerForSpecificMethod(kAnyHttpMethod, normalizedPath))
        return handler;
    return nullptr;
}

Handler* HandlerPool::findHandlerForSpecificMethod(
    const nx::network::http::Method& httpMethod, const QString& path) const
{
    const auto it = m_handlers.find(httpMethod);
    if (it == m_handlers.end())
        return nullptr;

    return findHandlerByPath(it->second, path);
}

Handler* HandlerPool::findHandlerByPath(
    const HandlersByPath& handlersByPath, const QString& normalizedPath)
{
    const QString decodedPath = QUrl::fromPercentEncoding(normalizedPath.toUtf8());
    auto it = handlersByPath.upper_bound(decodedPath);
    if (it == handlersByPath.begin())
        return decodedPath.startsWith(it->first) ? it->second.get() : nullptr;
    do
    {
        --it;
        if (decodedPath.startsWith(it->first))
            return it->second.get();
    } while (it != handlersByPath.begin());

    if (auto handler = handlersByPath.find(kAnyPath); handler != handlersByPath.end())
        return handler->second.get();

    return nullptr;
}

void HandlerPool::setPostprocessFunction(
    std::function<void()> postprocessFunction,
    const QString& path,
    const nx::network::http::Method& method)
{
    m_postprocessFunctions[std::make_pair(method, path)] = std::move(postprocessFunction);
}

void HandlerPool::executePostprocessFunction(
    const QString& path, const nx::network::http::Method& method)
{
    auto key = std::make_pair(method, path);
    if (auto it = m_postprocessFunctions.find(key); it != m_postprocessFunctions.end())
    {
        it->second();
        return;
    }

    key.first = kAnyHttpMethod;
    if (auto it = m_postprocessFunctions.find(key); it != m_postprocessFunctions.end())
        it->second();
}

void HandlerPool::auditFailedRequest(
    Handler* handler, const Request& request, const Response& response) const
{
    if (!NX_ASSERT(m_auditManager))
        return;

    if (!NX_ASSERT(!http::StatusCode::isSuccessCode(response.statusCode)))
        return;

    auto r = handler
        ? handler->prepareAuditRecord(request)
        : Handler::isAuditableRequest(request)
            ? (audit::Record) request
            : audit::Record{request.userSession};
    if (!r.apiInfo)
        return;

    QJsonObject object = r.apiInfo->toObject();
    object["status"] = response.statusCode;
    r.apiInfo = object;
    m_auditManager->addAuditRecord(std::move(r));
}

std::vector<std::pair<QString, QString>> HandlerPool::findHandlerOverlaps(
    const Request& request) const
{
    std::vector<std::pair<QString, QString>> result;
    const auto& method = request.method();
    if (auto it = m_crudHandlers.find(method); it != m_crudHandlers.end())
    {
        auto overlaps = it->second.findOverlaps(request);
        result.insert(result.end(),
            std::move_iterator(overlaps.begin()),
            std::move_iterator(overlaps.end()));
    }
    if (auto it = m_crudHandlers.find(kAnyHttpMethod); it != m_crudHandlers.end())
    {
        auto overlaps = it->second.findOverlaps(request);
        result.insert(result.end(),
            std::move_iterator(overlaps.begin()),
            std::move_iterator(overlaps.end()));
    }
    return result;
}

} // namespace nx::network::rest
