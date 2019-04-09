#pragma once

#include <type_traits>

#include <common/common_globals.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/exception.h>

#include "params.h"
#include "request.h"
#include "response.h"

namespace nx::network::rest {

enum class Permissions
{
    anyUser,
    adminOnly,
};

/**
 * Base class for REST requests processing.
 * Single handler instance receives all requests, each request in a different thread.
 */
class Handler
{
protected:
    /**
     * Override to implement request processing for all requests. Default implementation calls
     * exeute<METHOD>, override only them to process specific requests.
     * Implementation is expected to throw rest::Exception in case of error which is returned in
     * response.
     */
    virtual Response executeAnyMethod(const Request& request);

    virtual Response executeGet(const Request& request);
    virtual Response executeDelete(const Request& request);
    virtual Response executePost(const Request& request);
    virtual Response executePut(const Request& request);

public:
    Response executeRequest(const Request& request);

    /** Override to execute some logic after request processing. */
    virtual void afterExecute(const Request& request, const Response& response);

    friend class QnRestProcessorPool;

    GlobalPermission permissions() const;
    void setPath(const QString& path);
    void setPermissions(GlobalPermission permissions);
    QString extractAction(const QString& path) const;

    /** In derived classes, report all url params carrying camera id. */
    virtual QStringList cameraIdUrlParams() const;

protected:
    QString m_path;
    GlobalPermission m_permissions;
};

} // namespace nx::network::rest
