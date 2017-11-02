#pragma once

#include <nx/network/buffer.h>

namespace ec2 {

struct ApiSystemMergeHistoryRecord
{
    /** Millis since epoch. */
    qint64 timestamp;
    nx::String mergedSystemLocalId;
    nx::String mergedSystemCloudId;
    nx::String username;
    /**
     * base64(utc_timestamp_millis_decimal:MD5(cloud_system_id:utc_timestamp_millis_decimal:cloud_auth_key)).
     */
    nx::String signature;
};

#define ApiSystemMergeHistoryRecord_Fields \
    (timestamp)(mergedSystemLocalId)(mergedSystemCloudId)(username)(signature)

} // namespace ec2
