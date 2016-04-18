#pragma once

#include "api_data.h"

namespace ec2
{
    struct ApiUserGroupData : ApiIdData
    {
        ApiUserGroupData() :
            name(),
            permissions(Qn::NoGlobalPermissions) {}

        QString name;
        Qn::GlobalPermissions permissions;
    };
#define ApiUserGroupData_Fields ApiIdData_Fields (name)(permissions)

}