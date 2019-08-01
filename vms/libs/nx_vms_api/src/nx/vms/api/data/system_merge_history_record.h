#pragma once

#include "data.h"

#include <QtCore/QtGlobal>
#include <QtCore/QMetaType>
#include <QtCore/QByteArray>
#include <QtCore/QString>

#include <nx/utils/latin1_array.h>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API SystemMergeHistoryRecord: Data
{
    /** Milliseconds since epoch. */
    qint64 timestamp = 0;

    QByteArray mergedSystemLocalId;
    QByteArray mergedSystemCloudId;

    /** User who started merge. */
    QString username;

    /** Calculated with SystemMergeHistoryRecord::calculateSignature function. */
    QByteArray signature;

    void sign(const QByteArray& mergedSystemCloudAuthKey);
    bool verify(const QByteArray& mergedSystemCloudAuthKey) const;

    static QByteArray calculateSignature(
        const QByteArray& cloudSystemId,
        qint64 timestamp,
        const QByteArray& cloudAuthKey);
};

#define SystemMergeHistoryRecord_Fields \
    (timestamp) \
    (mergedSystemLocalId) \
    (mergedSystemCloudId) \
    (username) \
    (signature)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::SystemMergeHistoryRecord)
Q_DECLARE_METATYPE(nx::vms::api::SystemMergeHistoryRecordList)
