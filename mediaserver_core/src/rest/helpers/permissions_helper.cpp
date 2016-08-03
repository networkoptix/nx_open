#include "permissions_helper.h"

#include <media_server/settings.h>

#include <core/resource_management/resource_access_manager.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>

//This file is really placed in appserver2/src. Hate this.
#include <settings.h>

#include <rest/server/fusion_rest_handler.h>

#include <nx/network/http/httptypes.h>
#include <nx/utils/log/log.h>
#include "core/resource_management/user_access_data.h"

bool QnPermissionsHelper::isSafeMode()
{
    return MSSettings::roSettings()->value(nx_ms_conf::EC_DB_READ_ONLY).toInt()
        || ec2::Settings::instance()->dbReadOnly();
}

int QnPermissionsHelper::safeModeError(QnRestResult &result)
{
    auto errorMessage = lit("Can't process rest request because server is running in safe mode.");
    NX_LOG(errorMessage, cl_logDEBUG1);
    result.setError(QnJsonRestResult::CantProcessRequest, errorMessage);
    return nx_http::StatusCode::forbidden;
}

bool QnPermissionsHelper::hasOwnerPermissions(const QnUuid& id)
{
    if (id == Qn::kDefaultUserAccess.userId)
        return true; //< serve auth key authrozation

    auto userResource = qnResPool->getResourceById<QnUserResource>(id);
    return userResource && userResource->isOwner();
}

int QnPermissionsHelper::notOwnerError(QnRestResult &result)
{
    auto errorMessage = lit("Can't process rest request because authenticated user is not a system owner.");
    NX_LOG(errorMessage, cl_logDEBUG1);
    result.setError(QnJsonRestResult::CantProcessRequest, errorMessage);
    return nx_http::StatusCode::forbidden;
}

int QnPermissionsHelper::safeModeError(QByteArray& result, QByteArray& contentType, Qn::SerializationFormat format /*= Qn::UnsupportedFormat*/, bool extraFormatting /*= false*/)
{
    QnRestResult restResult;
    int errCode = QnPermissionsHelper::safeModeError(restResult);
    QnFusionRestHandlerDetail::serialize(restResult, result, contentType, format, extraFormatting);
    return errCode;
}

bool QnPermissionsHelper::hasGlobalPermission(const QnUuid& userId, Qn::GlobalPermission required)
{
    QnUserResourcePtr user = qnResPool->getResourceById<QnUserResource>(userId);
    if (!user)
        return false;

    return qnResourceAccessManager->hasGlobalPermission(user, required);
}

int QnPermissionsHelper::permissionsError(QnRestResult& result)
{
    auto errorMessage = lit("Can't process rest request because user has not enough access rights.");
    NX_LOG(errorMessage, cl_logDEBUG1);
    result.setError(QnJsonRestResult::CantProcessRequest, errorMessage);
    return nx_http::StatusCode::forbidden;
}

int QnPermissionsHelper::permissionsError(QByteArray& result, QByteArray& contentType, Qn::SerializationFormat format /*= Qn::UnsupportedFormat*/, bool extraFormatting /*= false*/)
{
    QnRestResult restResult;
    int errCode = QnPermissionsHelper::permissionsError(restResult);
    QnFusionRestHandlerDetail::serialize(restResult, result, contentType, format, extraFormatting);
    return errCode;
}
