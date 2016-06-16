#include "user_access_data.h"
#include <core/resource_management/resource_pool.h>
#include "core/resource/user_resource.h"

namespace Qn
{

QnUserResourcePtr getUserResourceByAccessData(const UserAccessData &userAccessData)
{
    auto resource = qnResPool->getResourceById(userAccessData.userId);
    if (!resource)
        return QnUserResourcePtr();
    return resource.dynamicCast<QnUserResource>();
}

}
