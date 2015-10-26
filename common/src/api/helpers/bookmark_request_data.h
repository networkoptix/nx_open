#pragma once

#include <QUrlQuery>

#include <api/helpers/request_helpers_fwd.h>
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
    QnVirtualCameraResourceList cameras;
};

struct QnGetBookmarkTagsRequestData: public QnMultiserverRequestData {
    QnGetBookmarkTagsRequestData(int limit = QnGetBookmarkTagsRequestData::unlimited());

    virtual void loadFromParams(const QnRequestParamList& params) override;
    virtual QnRequestParamList toParams() const override;
    virtual bool isValid() const override;

    static int unlimited();

    int limit;
};

struct QnUpdateBookmarkRequestData: public QnMultiserverRequestData {
    QnUpdateBookmarkRequestData();
    QnUpdateBookmarkRequestData(const QnCameraBookmark &bookmark);

    virtual void loadFromParams(const QnRequestParamList& params) override;
    virtual QnRequestParamList toParams() const override;
    virtual bool isValid() const override;

    QnCameraBookmark bookmark;
};

struct QnDeleteBookmarkRequestData: public QnMultiserverRequestData {
    QnDeleteBookmarkRequestData();
    QnDeleteBookmarkRequestData(const QnUuid &bookmarkId);

    virtual void loadFromParams(const QnRequestParamList& params) override;
    virtual QnRequestParamList toParams() const override;
    virtual bool isValid() const override;

    QnUuid bookmarkId;

};