// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

/**%apidoc Time-related data. */
struct QnTimeReply
{
    /**%apidoc Server time, in milliseconds since epoch. */
    qint64 utcTime;

    /**%apidoc Time zone offset, in milliseconds. */
    qint64 timeZoneOffset;

    /**%apidoc Identification of the time zone in the text form. */
    QString timezoneId;

    /**%apidoc Authentication realm. */
    QString realm;
};
#define QnTimeReply_Fields (utcTime)(timeZoneOffset)(timezoneId)(realm)

struct SyncTimeData
{
    qint64 utcTimeMs = 0; //< Utc time in milliseconds since epoch.
    bool isTakenFromInternet = false;
};
#define SyncTimeData_Fields (utcTimeMs)(isTakenFromInternet)

QN_FUSION_DECLARE_FUNCTIONS(QnTimeReply, (json)(ubjson)(xml)(csv_record), NX_VMS_COMMON_API)
QN_FUSION_DECLARE_FUNCTIONS(SyncTimeData, (json)(ubjson)(xml)(csv_record), NX_VMS_COMMON_API)
