#pragma once

#include <nx_ec/data/api_data.h>

namespace ec2
{
    struct ApiAccessRightsData: ApiData
    {
        QnUuid userId;
        QnUuid resourceId;
    };
#define ApiAccessRightsData_Fields (userId)(resourceId)

} // namespace ec2