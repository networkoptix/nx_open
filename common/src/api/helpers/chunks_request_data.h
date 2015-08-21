#pragma once

#include <QUrlQuery>

#include <core/resource/resource_fwd.h>
#include <utils/common/request_param.h>

struct QnChunksRequestData
{
    QnChunksRequestData();

    static QnChunksRequestData fromParams(const QnRequestParamList& params);
    QnRequestParamList toParams() const;
    QUrlQuery toUrlQuery() const;
    bool isValid() const;

    Qn::TimePeriodContent periodsType;
    QnVirtualCameraResourceList resList;
    qint64 startTimeMs;
    qint64 endTimeMs;
    qint64 detailLevel;
    QString filter;
    bool isLocal;
    Qn::SerializationFormat format;
    int limit;
    bool flat;
};
