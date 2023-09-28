// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QtGlobal>

#include <nx/utils/latin1_array.h>

#include "data_macros.h"

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API SystemMergeHistoryRecord
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
NX_VMS_API_DECLARE_STRUCT_AND_LIST(SystemMergeHistoryRecord)

} // namespace api
} // namespace vms
} // namespace nx
