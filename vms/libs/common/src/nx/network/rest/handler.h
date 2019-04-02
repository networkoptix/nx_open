#pragma once

#include <type_traits>

#include <common/common_globals.h>
#include <nx/fusion/model_functions.h>
#include <nx/utils/exception.h>
#include <rest/server/json_rest_result.h>

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
 * Single handler instance receives all requests, each request in different thread.
 */
class Handler: public QObject
{
public:
    virtual Response executeRequest(const Request& request);
    virtual void afterExecute(const Request& request, const Response& response);

    friend class QnRestProcessorPool;

    GlobalPermission permissions() const;
    void setPath(const QString& path);
    void setPermissions(GlobalPermission permissions) ;
    QString extractAction(const QString& path) const;

    /** In derived classes, report all url params carrying camera id. */
    virtual QStringList cameraIdUrlParams() const;

protected:
    virtual Response executeGet(const Request& request);
    virtual Response executeDelete(const Request& request);
    virtual Response executePost(const Request& request);
    virtual Response executePut(const Request& request);

protected:
    QString m_path;
    GlobalPermission m_permissions;
};

} // namespace nx::network::rest
