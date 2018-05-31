#pragma once

#include "api_data.h"

#include <nx/fusion/model_functions_fwd.h>

namespace ec2 {

struct ApiCloudSystemData: nx::vms::api::Data
{
    QnUuid localSystemId;
};

#define ApiCloudSystemData_Fields (localSystemId)

QN_FUSION_DECLARE_FUNCTIONS(ApiCloudSystemData, (json))

}

