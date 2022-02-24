// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <vector>

#include <QtCore/QMap>
#include <QtCore/QMetaType>
#include <QtCore/QString>

#include <nx/utils/uuid.h>
#include <nx/fusion/model_functions_fwd.h>

//!mediaserver's response to \a recording stats request
struct QnRecordingStatsData
{
    // Totals:
    qint64 recordedBytes = 0;    // recorded archive in bytes
    qint64 recordedSecs = 0;     // recorded archive in seconds (total)
    qint64 archiveDurationSecs = 0; // archive calendar duration in seconds

    // For requested time period:
    qint64 averageBitrate = 0;   // average bitrate (= bytes / sum of chunk duration in seconds)
    qint64 averageDensity = 0;   // average density (= bytes / requested period in seconds)

    QMap<QnUuid, qint64> recordedBytesPerStorage; // more detail data about recorded bytes

    void operator +=(const QnRecordingStatsData& right)
    {
        recordedBytes += right.recordedBytes;
        recordedSecs += right.recordedSecs;
        archiveDurationSecs += right.archiveDurationSecs;
    }

};
#define QnRecordingStatsData_Fields (recordedBytes)(recordedSecs)(archiveDurationSecs)(averageBitrate)(averageDensity)(recordedBytesPerStorage)

struct QnCamRecordingStatsData: public QnRecordingStatsData
{
    QnCamRecordingStatsData(): QnRecordingStatsData() {}
    QnCamRecordingStatsData(const QnRecordingStatsData& value): QnRecordingStatsData(value) {}

    QString physicalId;
};
#define QnCamRecordingStatsData_Fields (physicalId) QnRecordingStatsData_Fields

QN_FUSION_DECLARE_FUNCTIONS(QnRecordingStatsData, (json))
QN_FUSION_DECLARE_FUNCTIONS(QnCamRecordingStatsData, (json), NX_VMS_COMMON_API)

typedef QVector<QnCamRecordingStatsData> QnRecordingStatsReply;
Q_DECLARE_METATYPE(QnCamRecordingStatsData)
Q_DECLARE_METATYPE(QnRecordingStatsReply)
