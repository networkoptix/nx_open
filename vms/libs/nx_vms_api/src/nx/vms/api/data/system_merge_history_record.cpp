// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "system_merge_history_record.h"

#include <QtCore/QCryptographicHash>

#include <nx/fusion/model_functions.h>
#include <nx/utils/log/format.h>

namespace nx {
namespace vms {
namespace api {

void SystemMergeHistoryRecord::sign(const QByteArray& mergedSystemCloudAuthKey)
{
    signature = calculateSignature(
        mergedSystemCloudId,
        timestamp,
        mergedSystemCloudAuthKey);
}

bool SystemMergeHistoryRecord::verify(const QByteArray& mergedSystemCloudAuthKey) const
{
    return calculateSignature(
        mergedSystemCloudId,
        timestamp,
        mergedSystemCloudAuthKey) == signature;
}

QByteArray SystemMergeHistoryRecord::calculateSignature(
    const QByteArray& cloudSystemId,
    qint64 timestamp,
    const QByteArray& cloudAuthKey)
{
    const auto h1 = QCryptographicHash::hash(nx::format("%1:%2:%3")
        .args(cloudSystemId, timestamp, cloudAuthKey).toUtf8(), QCryptographicHash::Sha512)
        .toBase64();
    return nx::format("%1:%2").args(timestamp, h1).toUtf8().toBase64();
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(SystemMergeHistoryRecord,
    (ubjson)(xml)(json)(sql_record)(csv_record), SystemMergeHistoryRecord_Fields)

} // namespace api
} // namespace vms
} // namespace nx
