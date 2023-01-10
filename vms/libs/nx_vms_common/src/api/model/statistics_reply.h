// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>
#include <QtCore/QHash>
#include <QtCore/QQueue>

#include <common/common_globals.h>
#include <nx/fusion/model_functions_fwd.h>

struct QnStatisticsDataItem
{
    QnStatisticsDataItem() {}
    QnStatisticsDataItem(const QString &description,
                         qreal value,
                         Qn::StatisticsDeviceType deviceType,
                         int deviceFlags = 0):
        description(description),
        value(value),
        deviceType(deviceType),
        deviceFlags(deviceFlags){}

    QString description;
    qreal value;
    Qn::StatisticsDeviceType deviceType;
    int deviceFlags;
};

#define QnStatisticsDataItem_Fields (description)(value)(deviceType)(deviceFlags)

QN_FUSION_DECLARE_FUNCTIONS(QnStatisticsDataItem, (json))

typedef QList<QnStatisticsDataItem> QnStatisticsDataItemList;

struct QnStatisticsReply
{
    QnStatisticsReply(): updatePeriod(0), uptimeMs(0) {}

    QnStatisticsDataItemList statistics;
    qint64 updatePeriod;
    qint64 uptimeMs;
};

// TODO: #sivanov Move out, so can be included separately.
struct QnStatisticsData
{
    QString description;
    Qn::StatisticsDeviceType deviceType;
    QQueue<qreal> values;
};

typedef QHash<QString, QnStatisticsData> QnStatisticsHistory;

#define QnStatisticsReply_Fields (statistics)(updatePeriod)(uptimeMs)

QN_FUSION_DECLARE_FUNCTIONS(QnStatisticsReply, (json), NX_VMS_COMMON_API)
