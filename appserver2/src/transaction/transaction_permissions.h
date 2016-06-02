#pragma once

#include <common/common_globals.h>

#include <nx_ec/ec_api_fwd.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_access_manager.h>

#include <nx/utils/uuid.h>
#include "nx/utils/type_utils.h"
#include "nx_ec/data/api_statistics.h"

namespace ec2
{

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

inline bool hasPermission(const QnUuid &userId, const ApiResourceParamWithRefData &data, Qn::Permission permission)
{
    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
    QnResourcePtr target = qnResPool->getResourceById(data.resourceId);

    return qnResourceAccessManager->hasPermission(userResource, target, permission);
}

inline bool hasPermission(const QnUuid &/*userId*/, const ApiStoredFilePath &/*data*/, Qn::Permission /*permission*/)
{
    return true;
}

inline bool hasPermission(const QnUuid &/*userId*/, const ApiStoredFileData &/*data*/, Qn::Permission /*permission*/)
{
    return true;
}

inline bool hasPermission(const QnUuid &/*userId*/, const ApiClientInfoData &/*data*/, Qn::Permission /*permission*/)
{
    return true;
}

inline bool hasPermission(const QnUuid &/*userId*/, const ApiResourceTypeData &/*data*/, Qn::Permission /*permission*/)
{
    return true;
}
inline bool hasPermission(const QnUuid &/*userId*/, const ApiTransactionData &/*data*/, Qn::Permission /*permission*/)
{
    return true;
}

inline bool hasPermission(const QnUuid &userId, const ApiLicenseData &/*data*/, Qn::Permission /*permission*/)
{
    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
    return qnResourceAccessManager->hasGlobalPermission(userResource, Qn::GlobalPermission::GlobalAdminPermission);
}

inline bool hasPermission(const QnUuid &userId, const ApiLicenseOverflowData &/*data*/, Qn::Permission /*permission*/)
{
    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
    return qnResourceAccessManager->hasGlobalPermission(userResource, Qn::GlobalPermission::GlobalAdminPermission);
}

inline bool hasPermission(const QnUuid &userId, const ApiDatabaseDumpData &/*data*/, Qn::Permission /*permission*/)
{
    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
    return qnResourceAccessManager->hasGlobalPermission(userResource, Qn::GlobalPermission::GlobalAdminPermission);
}

inline bool hasPermission(const QnUuid &userId, const ApiVideowallControlMessageData &/*data*/, Qn::Permission /*permission*/)
{
    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
    return qnResourceAccessManager->hasGlobalPermission(userResource, Qn::GlobalControlVideoWallPermission);
    return true;
}

inline bool hasPermission(const QnUuid &userId, const ApiBusinessRuleData &/*data*/, Qn::Permission /*permission*/)
{
    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
    return qnResourceAccessManager->hasGlobalPermission(userResource, Qn::GlobalPermission::GlobalAdminPermission);
}

inline bool hasPermission(const QnUuid &userId, const ApiBusinessActionData &/*data*/, Qn::Permission permission)
{
    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
    if (permission == Qn::Permission::SavePermission)
        return qnResourceAccessManager->hasGlobalPermission(userResource, Qn::GlobalPermission::GlobalAdminPermission);
    else return true;
}

inline bool hasPermission(const QnUuid &userId, const ApiUpdateUploadData &/*data*/, Qn::Permission /*permission*/)
{
    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
    return qnResourceAccessManager->hasGlobalPermission(userResource, Qn::GlobalPermission::GlobalAdminPermission);
}

inline bool hasPermission(const QnUuid &userId, const ApiUpdateInstallData &/*data*/, Qn::Permission /*permission*/)
{
    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
    return qnResourceAccessManager->hasGlobalPermission(userResource, Qn::GlobalPermission::GlobalAdminPermission);
}

inline bool hasPermission(const QnUuid &userId, const ApiUpdateUploadResponceData &/*data*/, Qn::Permission /*permission*/)
{
    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
    return qnResourceAccessManager->hasGlobalPermission(userResource, Qn::GlobalPermission::GlobalAdminPermission);
}

inline bool hasPermission(const QnUuid &userId, const ApiPeerSystemTimeData &/*data*/, Qn::Permission /*permission*/)
{
    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
    return qnResourceAccessManager->hasGlobalPermission(userResource, Qn::GlobalPermission::GlobalAdminPermission);
}

inline bool hasPermission(const QnUuid &userId, const ApiSystemStatistics &/*data*/, Qn::Permission /*permission*/)
{
    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
    return qnResourceAccessManager->hasGlobalPermission(userResource, Qn::GlobalPermission::GlobalAdminPermission);
}

template<typename TransactionParamType>
bool hasPermission(const QnUuid &userId, const TransactionParamType &data, Qn::Permission permission);

template<template<typename...> class Container, typename TransactionParamType, typename... Tail>
void filterByPermission(const QnUuid &userId, Container<TransactionParamType, Tail...> &paramContainer, Qn::Permission permission)
{
    paramContainer.erase(std::remove_if(paramContainer.begin(), paramContainer.end(),
                                        [permission, &userId] (const TransactionParamType &param) {
                                            return !ec2::hasPermission(userId, param, permission);
                                        }), paramContainer.end());
}

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

template<template<typename...> class Container, typename TransactionParamType, typename... Tail>
auto hasPermissionImpl(const QnUuid &userId, const Container<TransactionParamType, Tail...> &data, Qn::Permission permission, int) -> bool
{
    Container<TransactionParamType, Tail...> contCopy = data;
    filterByPermission(userId, contCopy, permission);

    if (permission == Qn::Permission::ReadPermission && contCopy.size() == 0 && data.size() != 0)
        return false;
    else if (permission == Qn::Permission::SavePermission && contCopy.size() != data.size())
        return false;

    return true;
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

} // namespace ec2
