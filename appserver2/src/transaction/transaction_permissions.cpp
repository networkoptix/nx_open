#include "transaction_permissions.h"

#include <nx_ec/data/api_camera_history_data.h>

bool ec2::detail::hasPermissionImpl(const QnUuid &userId, const ApiResourceParamWithRefData &data, Qn::Permission permission)
{
    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
    QnResourcePtr target = qnResPool->getResourceById(data.resourceId);
    if (!target)
        return false;

    return qnResourceAccessManager->hasPermission(userResource, target, permission);
}

bool ec2::detail::hasPermissionImpl(const QnUuid &userId, const ApiSystemStatistics &/*data*/, Qn::Permission /*permission*/)
{
    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
    return qnResourceAccessManager->hasGlobalPermission(userResource, Qn::GlobalPermission::GlobalAdminPermission);
}

bool ec2::detail::hasPermissionImpl(const QnUuid &userId, const ApiPeerSystemTimeData &/*data*/, Qn::Permission permission)
{
    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
    if (permission == Qn::Permission::SavePermission)
        return qnResourceAccessManager->hasGlobalPermission(userResource, Qn::GlobalPermission::GlobalAdminPermission);
    else if (permission == Qn::Permission::ReadPermission)
        return true;
    else
        NX_ASSERT(0);
    return false;
}

bool ec2::detail::hasPermissionImpl(const QnUuid &userId, const ApiUpdateUploadResponceData &/*data*/, Qn::Permission /*permission*/)
{
    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
    return qnResourceAccessManager->hasGlobalPermission(userResource, Qn::GlobalPermission::GlobalAdminPermission);
}

bool ec2::detail::hasPermissionImpl(const QnUuid &userId, const ApiUpdateInstallData &/*data*/, Qn::Permission /*permission*/)
{
    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
    return qnResourceAccessManager->hasGlobalPermission(userResource, Qn::GlobalPermission::GlobalAdminPermission);
}

bool ec2::detail::hasPermissionImpl(const QnUuid &userId, const ApiUpdateUploadData &/*data*/, Qn::Permission /*permission*/)
{
    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
    return qnResourceAccessManager->hasGlobalPermission(userResource, Qn::GlobalPermission::GlobalAdminPermission);
}

bool ec2::detail::hasPermissionImpl(const QnUuid &userId, const ApiBusinessActionData &/*data*/, Qn::Permission permission)
{
    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
    if (permission == Qn::Permission::SavePermission)
        return qnResourceAccessManager->hasGlobalPermission(userResource, Qn::GlobalPermission::GlobalAdminPermission);
    return true;
}

bool ec2::detail::hasPermissionImpl(const QnUuid &userId, const ApiBusinessRuleData &/*data*/, Qn::Permission /*permission*/)
{
    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
    return qnResourceAccessManager->hasGlobalPermission(userResource, Qn::GlobalPermission::GlobalAdminPermission);
}

bool ec2::detail::hasPermissionImpl(const QnUuid &userId, const ApiLicenseData &/*data*/, Qn::Permission /*permission*/)
{
    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
    return qnResourceAccessManager->hasGlobalPermission(userResource, Qn::GlobalPermission::GlobalAdminPermission);
}

bool ec2::detail::hasPermissionImpl(const QnUuid &userId, const ApiUserGroupData&/*data*/, Qn::Permission permission)
{
    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
    if (permission == Qn::Permission::SavePermission)
        return qnResourceAccessManager->hasGlobalPermission(userResource, Qn::GlobalPermission::GlobalAdminPermission);
    else if (permission == Qn::Permission::ReadPermission)
        return true;
    else
        NX_ASSERT(0);
    return false;
}

bool ec2::detail::hasPermissionImpl(const QnUuid &userId, const ApiDiscoveredServerData&/*data*/, Qn::Permission permission)
{
    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
    if (permission == Qn::Permission::SavePermission)
        return qnResourceAccessManager->hasGlobalPermission(userResource, Qn::GlobalPermission::GlobalAdminPermission);
    else if (permission == Qn::Permission::ReadPermission)
        return true;
    else
        NX_ASSERT(0);
    return false;
}

bool ec2::detail::hasPermissionImpl(const QnUuid &userId, const ApiServerFootageData &data, Qn::Permission permission)
{
    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
    QnResourcePtr target = qnResPool->getResourceById(data.serverGuid);

    return qnResourceAccessManager->hasPermission(userResource, target, permission);
}

bool ec2::detail::hasPermissionImpl(const QnUuid &userId, const ApiCameraAttributesData &data, Qn::Permission permission)
{
    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
    QnResourcePtr target = qnResPool->getResourceById(data.cameraID);

    return qnResourceAccessManager->hasPermission(userResource, target, permission);
}

bool ec2::detail::hasPermissionImpl(const QnUuid &/*userId*/, const ApiTransactionData &/*data*/, Qn::Permission /*permission*/)
{
    return true;
}

bool ec2::detail::hasPermissionImpl(const QnUuid &/*userId*/, const ApiResourceTypeData &/*data*/, Qn::Permission /*permission*/)
{
    return true;
}

bool ec2::detail::hasPermissionImpl(const QnUuid &/*userId*/, const ApiClientInfoData &/*data*/, Qn::Permission /*permission*/)
{
    return true;
}

bool ec2::detail::hasPermissionImpl(const QnUuid &/*userId*/, const ApiStoredFileData &/*data*/, Qn::Permission /*permission*/)
{
    return true;
}

bool ec2::detail::hasPermissionImpl(const QnUuid &/*userId*/, const ApiStoredFilePath &/*data*/, Qn::Permission /*permission*/)
{
    return true;
}

bool ec2::detail::hasPermissionImpl(const QnUuid &userId, const ApiMediaServerUserAttributesData &data, Qn::Permission permission)
{
    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
    QnResourcePtr target = qnResPool->getResourceById(data.serverID);

    return qnResourceAccessManager->hasPermission(userResource, target, permission);
}

bool ec2::detail::hasPermissionImpl(const QnUuid &userId, const ApiAccessRightsData &/*data*/, Qn::Permission permission)
{
    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
    if (permission == Qn::Permission::SavePermission)
        return qnResourceAccessManager->hasGlobalPermission(userResource, Qn::GlobalPermission::GlobalAdminPermission);
    else if (permission == Qn::Permission::ReadPermission)
        return true;
    else
        NX_ASSERT(0);
    return false;
}

bool ec2::detail::hasPermissionImpl(const QnUuid &userId, const ApiUserData&/*data*/, Qn::Permission permission)
{
    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
    if (permission == Qn::Permission::SavePermission)
        return qnResourceAccessManager->hasGlobalPermission(userResource, Qn::GlobalPermission::GlobalAdminPermission);
    else if (permission == Qn::Permission::ReadPermission)
        return true;
    else
        NX_ASSERT(0);
    return false;
}

bool ec2::detail::hasPermissionImpl(const QnUuid &userId, const ApiLicenseOverflowData &/*data*/, Qn::Permission /*permission*/)
{
    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
    return qnResourceAccessManager->hasGlobalPermission(userResource, Qn::GlobalPermission::GlobalAdminPermission);
}

bool ec2::detail::hasPermissionImpl(const QnUuid &userId, const ApiDatabaseDumpData &/*data*/, Qn::Permission /*permission*/)
{
    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
    return qnResourceAccessManager->hasGlobalPermission(userResource, Qn::GlobalPermission::GlobalAdminPermission);
}

bool ec2::detail::hasPermissionImpl(const QnUuid &userId, const ApiVideowallControlMessageData &/*data*/, Qn::Permission /*permission*/)
{
    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
    return qnResourceAccessManager->hasGlobalPermission(userResource, Qn::GlobalControlVideoWallPermission);
    return true;
}
