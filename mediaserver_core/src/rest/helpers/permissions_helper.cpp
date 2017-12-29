#include "permissions_helper.h"

#include <media_server/settings.h>

#include <core/resource_access/resource_access_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>

//This file is really placed in appserver2/src. Hate this.
#include <settings.h>

#include <rest/server/fusion_rest_handler.h>

#include <nx/network/http/http_types.h>
#include <nx/utils/log/log.h>
#include "core/resource_access/user_access_data.h"
#include <media_server/media_server_module.h>

bool QnPermissionsHelper::isSafeMode(const QnCommonModule* commonModule)
{
    return qnServerModule->roSettings()->value(nx_ms_conf::EC_DB_READ_ONLY).toInt()
        || commonModule->isReadOnly();
}

int QnPermissionsHelper::safeModeError(QnRestResult &result)
{
    auto errorMessage = lit("Can't process rest request because server is running in safe mode.");
    NX_LOG(errorMessage, cl_logDEBUG1);
    result.setError(QnJsonRestResult::CantProcessRequest, errorMessage);
    return nx::network::http::StatusCode::forbidden;
}

bool QnPermissionsHelper::hasOwnerPermissions(
    QnResourcePool* resPool,
    const Qn::UserAccessData& accessRights)
{
    if (accessRights == Qn::kSystemAccess)
        return true; //< serve auth key authrozation

    auto userResource = resPool->getResourceById<QnUserResource>(accessRights.userId);
    return userResource && userResource->isOwner();
}

int QnPermissionsHelper::notOwnerError(QnRestResult &result)
{
    auto errorMessage = lit("Can't process rest request because authenticated user is not a system owner.");
    NX_LOG(errorMessage, cl_logDEBUG1);
    result.setError(QnJsonRestResult::CantProcessRequest, errorMessage);
    return nx::network::http::StatusCode::forbidden;
}

int QnPermissionsHelper::safeModeError(QByteArray& result, QByteArray& contentType, Qn::SerializationFormat format /*= Qn::UnsupportedFormat*/, bool extraFormatting /*= false*/)
{
    QnRestResult restResult;
    int errCode = QnPermissionsHelper::safeModeError(restResult);
    QnFusionRestHandlerDetail::serialize(restResult, result, contentType, format, extraFormatting);
    return errCode;
}

int QnPermissionsHelper::permissionsError(QnRestResult& result)
{
    auto errorMessage = lit("Can't process rest request because user has not enough access rights.");
    NX_LOG(errorMessage, cl_logDEBUG1);
    result.setError(QnJsonRestResult::CantProcessRequest, errorMessage);
    return nx::network::http::StatusCode::forbidden;
}

int QnPermissionsHelper::permissionsError(QByteArray& result, QByteArray& contentType, Qn::SerializationFormat format /*= Qn::UnsupportedFormat*/, bool extraFormatting /*= false*/)
{
    QnRestResult restResult;
    int errCode = QnPermissionsHelper::permissionsError(restResult);
    QnFusionRestHandlerDetail::serialize(restResult, result, contentType, format, extraFormatting);
    return errCode;
}
