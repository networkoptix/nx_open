#pragma once

#include <QUrlQuery>

#include <core/resource/resource_fwd.h>
#include <utils/common/request_param.h>

struct QnChunksRequestData
{
    // Versions here are server versions.
    enum class RequestVersion
    {
        v2_6,
        v3_0,

        current = v3_0
    };

    QnChunksRequestData();

    static QnChunksRequestData fromParams(const QnRequestParamList& params);
    QnRequestParamList toParams() const;
    QUrlQuery toUrlQuery() const;
    bool isValid() const;

    RequestVersion requestVersion = RequestVersion::current;

    Qn::TimePeriodContent periodsType;
    QnSecurityCamResourceList resList;
    qint64 startTimeMs;
    qint64 endTimeMs;
    qint64 detailLevel;
    bool keepSmallChunks;
    QString filter;
    bool isLocal;
    Qn::SerializationFormat format;
    int limit;
    bool flat;
};
