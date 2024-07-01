// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/network/http/http_types.h>
#include <nx/utils/auth/global_permission.h>
#include <nx/utils/exception.h>
#include <nx/utils/scope_guard.h>

#include "audit.h"
#include "open_api_schema.h"
#include "params.h"
#include "path_router.h"
#include "request.h"
#include "response.h"

namespace nx::network { class AbstractStreamSocket; }

namespace nx::network::rest {

using DeviceIdRetriever = std::function<QString(const nx::network::http::Request&)>;

/**
 * Base class for REST requests processing.
 * Single handler instance receives all requests, each request in a different thread.
 */
class NX_NETWORK_REST_API Handler
{
protected:
    /**
     * Customization point to validate and modify the request. By default the request will be
     * validated against OpenAPI schemas.
     * @param request
     * @param headers HTTP headers to be populated upon warnings or errors occurred during
     * validation
     */
    virtual void validateAndAmend(Request *request, http::HttpHeaders* headers);

    /**
     * Override to implement request processing for all requests. Default implementation calls
     * execute<METHOD>, override only them to process specific requests. Implementation is expected
     * to throw rest::Exception in case of error which is handled by an executeRequestOrThrow's
     * caller.
     */
    virtual Response executeAnyMethod(const Request& request);
    virtual Response executeGet(const Request& request);
    virtual Response executeDelete(const Request& request);
    virtual Response executePost(const Request& request);
    virtual Response executePut(const Request& request);
    virtual Response executePatch(const Request& request);

public:
    using GlobalPermission = nx::utils::auth::GlobalPermission;

    Handler(
        GlobalPermission readPermissions = GlobalPermission::none,
        GlobalPermission modifyPermissions = GlobalPermission::none);
    virtual ~Handler() = default;

    /**
     * @param headers Ad hoc parameter to ensure that warnings are added to the response
     * even in case of an exception.</br>
     * Consider changing the control flow by moving try-catch block inside the `executeRequest` and
     * calling `validateAndAmend` before the `try`.
     */
    Response executeRequestOrThrow(Request* request, http::HttpHeaders* headers = nullptr);

    /**
     * Override to execute some logic after request processing. socket is not empty only if
     * response.isUndefinedContentLength is true set by any execute... methods.
     */
    virtual void afterExecute(
        const Request& request,
        const Response& response,
        std::unique_ptr<AbstractStreamSocket> socket);

    GlobalPermission readPermissions() const;
    void setReadPermissions(GlobalPermission permissions);

    GlobalPermission modifyPermissions() const;
    void setModifyPermissions(GlobalPermission permissions);

    void setPath(const QString& path);
    void setSchemas(std::shared_ptr<json::OpenApiSchemas> schemas);
    void setAuditManager(audit::Manager* auditManager) { m_auditManager = auditManager; }

    QString extractAction(const QString& path) const;

    /** In derived classes, report all url params carrying camera id. */
    virtual QStringList cameraIdUrlParams() const;

    virtual DeviceIdRetriever createCustomDeviceIdRetriever() const { return {}; }

    virtual void modifyPathRouteResultOrThrow(PathRouter::Result*) const {}

    virtual bool isConcreteIdProvidedInPath(PathRouter::Result*) const { return false; }

    virtual void clarifyApiVersionOrThrow(const PathRouter::Result&, Request*) const {}

    virtual std::string_view corsMethodsAllowed(const nx::network::http::Method& method) const;

    virtual void pleaseStop() { m_needStop = true; }

    enum class NotifyType
    {
        update,
        delete_
    };

    using SubscriptionCallback =
        nx::utils::MoveOnlyFunc<void(const QString& id, NotifyType, QJsonValue payload)>;

    virtual nx::utils::Guard subscribe(const QString& /*id*/, const Request&, SubscriptionCallback)
    {
        return {};
    }

    virtual QString idParamName() const { return {}; }
    virtual QString subscriptionId(const Request&) { return {}; }
    virtual void audit(const Request& request, const Response& response);
    virtual audit::Record prepareAuditRecord(const Request& request) const;
    static bool isAuditableRequest(const Request& request);

protected:
    std::atomic<bool> m_needStop = false;
    QString m_path;
    GlobalPermission m_readPermissions;
    GlobalPermission m_modifyPermissions;

    // TODO:
    //   Consider moving schemas (and validating requests/responses) to the URI Router class.
    //   Also, a lot of checks can be performed implicitly by code generation from an OpenAPI
    //   schema file.
    std::shared_ptr<json::OpenApiSchemas> m_schemas;
    audit::Manager* m_auditManager = nullptr;
};

} // namespace nx::network::rest
