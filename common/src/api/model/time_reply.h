#pragma once

#include <QtCore/QMetaType>

#include <nx/fusion/model_functions_fwd.h>

#include <nx/utils/uuid.h>

struct QnTimeReply
{
    /** Utc time in msecs since epoch. */
    qint64 utcTime;

    /** Time zone offset, in msecs. */
    qint64 timeZoneOffset;
    QString timezoneId;
    QString realm;
};
#define QnTimeReply_Fields (utcTime)(timeZoneOffset)(timezoneId)(realm)

struct ApiDateTimeData
{
    qint64 timeSinseEpochMs = 0;
    QString timeZoneId;
    qint64 timeZoneOffsetMs = 0;
};
#define ApiDateTimeData_Fields (timeSinseEpochMs)(timeZoneId)(timeZoneOffsetMs)

struct ApiServerDateTimeData: public ApiDateTimeData
{
    QnUuid serverId;
};
#define ApiServerDateTimeData_Fields ApiDateTimeData_Fields (serverId)

using ApiMultiserverServerDateTimeDataList = std::vector<ApiServerDateTimeData>;

QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((QnTimeReply)(ApiDateTimeData)(ApiServerDateTimeData), (json)(ubjson)(xml)(csv_record)(metatype))
