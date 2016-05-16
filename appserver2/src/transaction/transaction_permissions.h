#pragma once

#include <common/common_globals.h>

#include <nx_ec/ec_api_fwd.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_access_manager.h>

#include <nx/utils/uuid.h>

namespace ec2
{
    template<typename TransactionParamType>
    bool hasPermission(const QnUuid &userId, const TransactionParamType &data, Qn::Permission permission)
    {
        /* Check if resource needs to be created. */
        QnResourcePtr target = qnResPool->getResourceById(data.id);
        if (!target)
            return qnResourceAccessManager->canCreateResource(userId, data);

        return qnResourceAccessManager->hasPermission(user, target, permission);
    }
}
