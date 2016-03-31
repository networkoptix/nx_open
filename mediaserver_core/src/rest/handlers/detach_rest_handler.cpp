#include "detach_rest_handler.h"

#include <api/global_settings.h>

#include <common/common_module.h>

#include <nx/network/http/httptypes.h>
#include "media_server/serverutil.h"

#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>

namespace {
    static const QString kDefaultAdminPassword = "admin";
}

int QnDetachFromSystemRestHandler::executeGet(const QString &, const QnRequestParams & params, QnJsonRestResult &result, const QnRestConnectionProcessor*)
{
    return execute(std::move(PasswordData(params)), result);
}

int QnDetachFromSystemRestHandler::executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result, const QnRestConnectionProcessor*)
{
    Q_UNUSED(path);
    Q_UNUSED(params);

    PasswordData passwordData = QJson::deserialized<PasswordData>(body);
    return execute(std::move(passwordData), result);
}

int QnDetachFromSystemRestHandler::execute(PasswordData passwordData, QnJsonRestResult &result)
{
    if (!passwordData.hasPassword())
        passwordData.password = kDefaultAdminPassword;

    QString errStr;
    if (!validatePasswordData(passwordData, &errStr))
    {
        result.setError(QnJsonRestResult::CantProcessRequest, errStr);
        return nx_http::StatusCode::ok;
    }
    const auto admin = qnResPool->getAdministrator();
    if (!admin)
    {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("Internal server error. Can't find admin user."));
        return nx_http::StatusCode::ok;
    }

    if (!changeAdminPassword(passwordData)) {
        result.setError(QnJsonRestResult::CantProcessRequest, lit("Internal server error. Can't change password."));
        return nx_http::StatusCode::ok;
    }

    qnGlobalSettings->resetCloudParams();
    qnGlobalSettings->setNewSystem(true);
    if (!qnGlobalSettings->synchronizeNowSync())
    {
        result.setError(
            QnJsonRestResult::CantProcessRequest,
            lit("Failed to save cloud credentials to local DB"));
        return nx_http::StatusCode::ok;
    }
    qnCommon->updateModuleInformation();

    nx::SystemName systemName;
    systemName.resetToDefault();
    if (!changeSystemName(systemName, 0, 0))
    {
        result.setError(QnRestResult::CantProcessRequest, lit("Internal server error.  Can't change system name."));
        return nx_http::StatusCode::internalServerError;
    }

    return nx_http::StatusCode::ok;
}
