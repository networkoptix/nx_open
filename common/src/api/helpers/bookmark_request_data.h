#pragma once

#include <api/helpers/request_helpers_fwd.h>
#include <api/helpers/multiserver_request_data.h>

#include <core/resource/camera_bookmark.h>
#include <core/resource/resource_fwd.h>
#include <utils/common/request_param.h>
#include <nx/vms/event/event_fwd.h>

struct QnGetBookmarksRequestData: public QnMultiserverRequestData
{
    QnGetBookmarksRequestData();

    virtual void loadFromParams(QnResourcePool* resourcePool,
        const QnRequestParamList& params) override;
    virtual QnRequestParamList toParams() const override;
    virtual bool isValid() const override;

    QnCameraBookmarkSearchFilter filter;
    QnSecurityCamResourceList cameras;
};

struct QnGetBookmarkTagsRequestData: public QnMultiserverRequestData
{
    explicit QnGetBookmarkTagsRequestData(int limit = unlimited());

    virtual void loadFromParams(QnResourcePool* resourcePool,
        const QnRequestParamList& params) override;
    virtual QnRequestParamList toParams() const override;
    virtual bool isValid() const override;

    static int unlimited();

    int limit;
};

struct QnUpdateBookmarkRequestData: public QnMultiserverRequestData
{
    QnUpdateBookmarkRequestData();
    QnUpdateBookmarkRequestData(const QnCameraBookmark& bookmark);
    QnUpdateBookmarkRequestData(
        const QnCameraBookmark& bookmark,
        const nx::vms::event::AbstractActionPtr& action);

    virtual void loadFromParams(QnResourcePool* resourcePool,
        const QnRequestParamList& params) override;
    virtual QnRequestParamList toParams() const override;
    virtual bool isValid() const override;

    QnCameraBookmark bookmark;
    QnUuid businessRuleId;
    nx::vms::event::EventType eventType;
};

struct QnDeleteBookmarkRequestData: public QnMultiserverRequestData
{
    QnDeleteBookmarkRequestData();
    QnDeleteBookmarkRequestData(const QnUuid& bookmarkId);

    virtual void loadFromParams(QnResourcePool* resourcePool,
        const QnRequestParamList& params) override;
    virtual QnRequestParamList toParams() const override;
    virtual bool isValid() const override;

    QnUuid bookmarkId;
};
