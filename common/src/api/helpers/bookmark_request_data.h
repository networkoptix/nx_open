#pragma once

#include <QUrlQuery>

#include <core/resource/camera_bookmark.h>
#include <core/resource/resource_fwd.h>
#include <utils/common/request_param.h>

struct QnBookmarkRequestData
{
    QnBookmarkRequestData();

    static QnBookmarkRequestData fromParams(const QnRequestParamList& params);
    QnRequestParamList toParams() const;
    QUrlQuery toUrlQuery() const;
    bool isValid() const;

    QnCameraBookmarkSearchFilter filter;
    Qn::SerializationFormat format;
    QnVirtualCameraResourceList cameras;
    bool isLocal;   
};
