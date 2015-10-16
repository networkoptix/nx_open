#pragma once

#include <QUrlQuery>

#include <api/helpers/multiserver_request_data.h>

#include <core/resource/camera_bookmark.h>
#include <core/resource/resource_fwd.h>
#include <utils/common/request_param.h>

struct QnGetBookmarksRequestData: public QnMultiserverRequestData {
    QnGetBookmarksRequestData();

    virtual void loadFromParams(const QnRequestParamList& params) override;
    virtual QnRequestParamList toParams() const override;
    virtual bool isValid() const override;

    QnCameraBookmarkSearchFilter filter;
    Qn::SerializationFormat format;
    QnVirtualCameraResourceList cameras;
};

struct QnUpdateBookmarkRequestData: public QnMultiserverRequestData {
    QnUpdateBookmarkRequestData();

    virtual void loadFromParams(const QnRequestParamList& params) override;
    virtual QnRequestParamList toParams() const override;
    virtual bool isValid() const override;

    QnCameraBookmark bookmark;
};

struct QnDeleteBookmarkRequestData: public QnMultiserverRequestData {
    QnDeleteBookmarkRequestData();

    virtual void loadFromParams(const QnRequestParamList& params) override;
    virtual QnRequestParamList toParams() const override;
    virtual bool isValid() const override;

    QnUuid bookmarkId;

};