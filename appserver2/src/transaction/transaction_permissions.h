#pragma once

#include <common/common_globals.h>

#include <nx_ec/ec_api_fwd.h>

#include <core/resource_management/resource_pool.h>
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
    /* Check if resource needs to be created. */
    QnResourcePtr target = qnResPool->getResourceById(data.id);
    if (!target)
        return qnResourceAccessManager->canCreateResource(userId, data);

    auto userResource = qnResPool->getResourceById(userId).dynamicCast<QnUserResource>();
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

} // namespace ec2
