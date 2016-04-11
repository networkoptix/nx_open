#pragma once

#include <nx_ec/data/api_data.h>

namespace ec2
{
    struct ApiAccessRightsData: ApiData
    {
        QnUuid userId;
        std::vector<QnUuid> resourceIds;
    };
#define ApiAccessRightsData_Fields (userId)(resourceIds)

} // namespace ec2
