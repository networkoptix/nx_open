#pragma once

#include <QtGlobal>

#include "api_data.h"
#include "api_fwd.h"


namespace ec2 {

struct ApiPeerSyncTimeData: public nx::vms::api::Data
{
    qint64 syncTimeMs = 0;
};
#define ApiPeerSyncTimeData_Fields (syncTimeMs)

} // namespace ec2
