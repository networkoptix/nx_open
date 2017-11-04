#pragma once

#include <nx/network/buffer.h>

namespace ec2 {

struct ApiSystemMergeHistoryRecord
{
    /** Millis since epoch. */
    qint64 timestamp;
    nx::String mergedSystemLocalId;
    nx::String mergedSystemCloudId;
    QString username;
    /** Calculated with ApiSystemMergeHistoryRecord::calculateSignature function. */
    nx::String signature;

    void sign(const nx::String& mergedSystemCloudAuthKey);
    bool verify(const nx::String& mergedSystemCloudAuthKey) const;

    static nx::String calculateSignature(
        const nx::String& cloudSystemId,
        qint64 timestamp,
        const nx::String& cloudAuthKey);
};

#define ApiSystemMergeHistoryRecord_Fields \
    (timestamp)(mergedSystemLocalId)(mergedSystemCloudId)(username)(signature)

} // namespace ec2
