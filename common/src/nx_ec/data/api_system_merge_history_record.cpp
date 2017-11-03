#include "api_system_merge_history_record.h"

#include <nx/fusion/model_functions.h>

namespace ec2 {

nx::String ApiSystemMergeHistoryRecord::calculateSignature(
    const nx::String& cloudSystemId,
    qint64 timestamp,
    const nx::String& cloudAuthKey)
{
    //base64(utc_timestamp_millis_decimal:base64(MD5(cloud_system_id:utc_timestamp_millis_decimal:cloud_auth_key))).
    const auto h1 = QCryptographicHash::hash(lm("%1:%2:%3")
        .args(cloudSystemId, timestamp, cloudAuthKey).toUtf8(), QCryptographicHash::Md5)
        .toBase64();
    return lm("%1:%2").args(timestamp, h1).toUtf8().toBase64();
}

void ApiSystemMergeHistoryRecord::signRecord(
    ApiSystemMergeHistoryRecord* record,
    const nx::String& mergedSystemCloudAuthKey)
{
    record->signature = calculateSignature(
        record->mergedSystemCloudId,
        record->timestamp,
        mergedSystemCloudAuthKey);
}

bool ApiSystemMergeHistoryRecord::verifyRecordSignature(
    const ApiSystemMergeHistoryRecord& record,
    const nx::String& mergedSystemCloudAuthKey)
{
    return calculateSignature(
        record.mergedSystemCloudId,
        record.timestamp,
        mergedSystemCloudAuthKey) == record.signature;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ApiSystemMergeHistoryRecord),
    (ubjson)(xml)(json)(sql_record)(csv_record),
    _Fields)

} // namespace ec2
