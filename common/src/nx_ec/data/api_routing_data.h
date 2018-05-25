#pragma once

#include "api_data.h"

namespace ec2 {

struct ApiConnectionData: nx::vms::api::Data
{
    ApiConnectionData() = default;

    QnUuid peerId;
    QString host;
    int port = 0;

    bool operator==(const ApiConnectionData &other) const
    {
        return peerId == other.peerId
            && host == other.host
            && port == other.port;
    }
};
#define ApiConnectionData_Fields (peerId)(host)(port)

} // namespace ec2
