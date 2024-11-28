// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <optional>

#include "handler.h"
#include "user_access_data.h"

namespace nx::network::rest {

class NX_NETWORK_REST_API HandlerPool
{
    using HandlersByPath = std::map<QString, std::unique_ptr<Handler>>;
    using Handlers = std::map<nx::network::http::Method, HandlersByPath>;
    using RedirectRules = QMap<QString, QString>;

public:
    using GlobalPermission = Handler::GlobalPermission;

    static const nx::network::http::Method kAnyHttpMethod;
    static const QString kAnyPath;

    HandlerPool() = default;
    HandlerPool(const HandlerPool&) = delete;
    HandlerPool& operator=(const HandlerPool&) = delete;

    void registerHandler(
        const QString& path,
        GlobalPermission readPermissions,
        GlobalPermission modifyPermissions,
        std::unique_ptr<Handler> handler,
        const nx::network::http::Method& method = kAnyHttpMethod);

    void registerHandler(
        const QString& path,
        GlobalPermission permissions,
        std::unique_ptr<Handler> handler,
        const nx::network::http::Method& method = kAnyHttpMethod);

    /** @deprecated */
    void registerHandler(
        const QString& path,
        Handler* handler,
        GlobalPermission permission = GlobalPermission::none);

    /** @deprecated */
    void registerHandler(
        const nx::network::http::Method& httpMethod,
        const QString& path,
        Handler* handler,
        GlobalPermission permission = GlobalPermission::none);

    Handler* findHandlerOrThrow(
        Request* request, const QString& pathIgnorePrefix = QString()) const;

    Handler* findCrudHandlerOrThrow(
        Request* request, const QString& pathIgnorePrefix = QString()) const;

    void registerRedirectRule(const QString& path, const QString& newPath);
    std::optional<QString> getRedirectRule(const QString& path);

    void setSchemas(std::shared_ptr<json::OpenApiSchemas> schemas);
    void setAuditManager(audit::Manager* auditManager) { m_auditManager = auditManager; }

    Handler* findHandler(
        const nx::network::http::Method& httpMethod, const QString& normalizedPath) const;

    Handler* findHandlerForSpecificMethod(
        const nx::network::http::Method& httpMethod,
        const QString& normalizedPath) const;

    static Handler* findHandlerByPath(
        const HandlersByPath& handlersByPath,
        const QString& normalizedPath);

    void setPostprocessFunction(
        std::function<void()> postprocessFunction,
        const QString& path,
        const nx::network::http::Method& method = HandlerPool::kAnyHttpMethod);

    void executePostprocessFunction(const QString& path, const nx::network::http::Method& method);

    void auditFailedRequest(
        Handler* handler, const Request& request, const Response& response) const;

    auto createRegisterHandlerFunctor()
    {
        return
            [this](const std::string_view& path, auto&&... args)
            {
                registerHandler(
                    PathRouter::replaceVersionWithRegex(path),
                    std::forward<decltype(args)>(args)...);
            };
    }

    std::vector<std::pair<QString, QString>> findHandlerOverlaps(
        const Request& request) const;

private:
    /**
     * NOTE: Empty method name means any method.
     */
    Handlers m_handlers;
    RedirectRules m_redirectRules;
    std::map<nx::network::http::Method, PathRouter> m_crudHandlers;
    std::shared_ptr<json::OpenApiSchemas> m_crudSchemas;
    audit::Manager* m_auditManager = nullptr;
    std::map<std::pair<nx::network::http::Method, QString>, std::function<void()>>
        m_postprocessFunctions;
};

} // namespace nx::network::rest
