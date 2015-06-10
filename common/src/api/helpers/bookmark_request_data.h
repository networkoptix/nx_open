#pragma once

#include <QUrlQuery>

#include <core/resource/resource_fwd.h>
#include <utils/common/request_param.h>

struct QnBookmarkRequestData
{
    QnBookmarkRequestData();

    static QnBookmarkRequestData fromParams(const QnRequestParamList& params);
    QnRequestParamList toParams() const;
    QUrlQuery toUrlQuery() const;
    bool isValid() const;

    Qn::BookmarkSearchStrategy strategy;
    Qn::SerializationFormat format;

    QnVirtualCameraResourceList cameras;
    qint64 startTimeMs;
    qint64 endTimeMs;
    QString filter;
    bool isLocal;   
    int limit;
};
