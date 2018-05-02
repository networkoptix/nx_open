#include "api_system_merge_history_record.h"

#include <nx/fusion/model_functions.h>

namespace ec2 {

void ApiSystemMergeHistoryRecord::sign(
    const nx::String& mergedSystemCloudAuthKey)
{
    signature = calculateSignature(
        mergedSystemCloudId,
        timestamp,
        mergedSystemCloudAuthKey);
}

bool ApiSystemMergeHistoryRecord::verify(
    const nx::String& mergedSystemCloudAuthKey) const
{
    return calculateSignature(
        mergedSystemCloudId,
        timestamp,
        mergedSystemCloudAuthKey) == signature;
}

nx::String ApiSystemMergeHistoryRecord::calculateSignature(
    const nx::String& cloudSystemId,
    qint64 timestamp,
    const nx::String& cloudAuthKey)
{
    //base64(utc_timestamp_millis_decimal:base64(MD5(cloud_system_id:utc_timestamp_millis_decimal:cloud_auth_key))).
    const auto h1 = QCryptographicHash::hash(lm("%1:%2:%3")
        .args(cloudSystemId, timestamp, cloudAuthKey).toUtf8(), QCryptographicHash::Sha512)
        .toBase64();
    return lm("%1:%2").args(timestamp, h1).toUtf8().toBase64();
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ApiSystemMergeHistoryRecord),
    (ubjson)(xml)(json)(sql_record)(csv_record),
    _Fields)

} // namespace ec2
