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

    static nx::String calculateSignature(
        const nx::String& cloudSystemId,
        qint64 timestamp,
        const nx::String& cloudAuthKey);

    static void signRecord(
        ApiSystemMergeHistoryRecord* record,
        const nx::String& mergedSystemCloudAuthKey);

    static bool verifyRecordSignature(
        const ApiSystemMergeHistoryRecord& record,
        const nx::String& mergedSystemCloudAuthKey);
};

#define ApiSystemMergeHistoryRecord_Fields \
    (timestamp)(mergedSystemLocalId)(mergedSystemCloudId)(username)(signature)

} // namespace ec2
