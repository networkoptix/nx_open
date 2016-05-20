#pragma once

#include <common/common_globals.h>

#include <nx_ec/ec_api_fwd.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_access_manager.h>

#include <nx/utils/uuid.h>
#include "nx/utils/type_utils.h"

namespace ec2
{
namespace detail
{

template<typename TransactionParamType>
auto hasPermissionImpl(const QnUuid &userId, const TransactionParamType &data, Qn::Permission permission, int) -> nx::utils::SfinaeCheck<decltype(data.id), bool>
{
    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();

    /* Check if resource needs to be created. */
    QnResourcePtr target = qnResPool->getResourceById(data.id);
    if (!target)
        return qnResourceAccessManager->canCreateResource(userResource, data);

    return qnResourceAccessManager->hasPermission(userResource, target, permission);
}

template<typename TransactionParamType>
auto hasPermissionImpl(const QnUuid &/*userId*/, const TransactionParamType &/*data*/, Qn::Permission /*permission*/, char) -> bool
{
    return true;
}

} // namespace detail

template<typename TransactionParamType>
bool hasPermission(const QnUuid &userId, const TransactionParamType &data, Qn::Permission permission)
{
    return detail::hasPermissionImpl(userId, data, permission, 0);
}

inline bool hasPermission(const QnUuid &userId, const ApiServerFootageData &data, Qn::Permission permission)
{
    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
    QnResourcePtr target = qnResPool->getResourceById(data.serverGuid);

    return qnResourceAccessManager->hasPermission(userResource, target, permission);
}

inline bool hasPermission(const QnUuid &userId, const ApiCameraAttributesData &data, Qn::Permission permission)
{
    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
    QnResourcePtr target = qnResPool->getResourceById(data.cameraID);

    return qnResourceAccessManager->hasPermission(userResource, target, permission);
}

inline bool hasPermission(const QnUuid &userId, const ApiMediaServerUserAttributesData &data, Qn::Permission permission)
{
    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
    QnResourcePtr target = qnResPool->getResourceById(data.serverID);

    return qnResourceAccessManager->hasPermission(userResource, target, permission);
}

inline bool hasPermission(const QnUuid &userId, const ApiAccessRightsData &data, Qn::Permission permission)
{
    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
    QnResourcePtr target = qnResPool->getResourceById(data.userId);

    return qnResourceAccessManager->hasPermission(userResource, target, permission);
}

inline bool hasPermission(const QnUuid &userId, const ApiStoredFilePath &data, Qn::Permission permission)
{
//    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
//    QnResourcePtr target = qnResPool->getResourceById(data.userId);

//    return qnResourceAccessManager->hasPermission(userResource, target, permission);
    // TODO: implement
}

inline bool hasPermission(const QnUuid &userId, const ApiStoredFileData &data, Qn::Permission permission)
{
//    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
//    QnResourcePtr target = qnResPool->getResourceById(data.userId);

//    return qnResourceAccessManager->hasPermission(userResource, target, permission);
    // TODO: implement
}

inline bool hasPermission(const QnUuid &userId, const ApiLicenseData &data, Qn::Permission permission)
{
//    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
//    QnResourcePtr target = qnResPool->getResourceById(data.userId);

//    return qnResourceAccessManager->hasPermission(userResource, target, permission);
    // TODO: implement
}

inline bool hasPermission(const QnUuid &userId, const ApiLicenseOverflowData &data, Qn::Permission permission)
{
//    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
//    QnResourcePtr target = qnResPool->getResourceById(data.userId);

//    return qnResourceAccessManager->hasPermission(userResource, target, permission);
    // TODO: implement
}

inline bool hasPermission(const QnUuid &userId, const ApiDatabaseDumpData &data, Qn::Permission permission)
{
//    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
//    QnResourcePtr target = qnResPool->getResourceById(data.userId);

//    return qnResourceAccessManager->hasPermission(userResource, target, permission);
    // TODO: implement
}
} // namespace ec2
