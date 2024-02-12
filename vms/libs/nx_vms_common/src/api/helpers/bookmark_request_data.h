// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <api/helpers/multiserver_request_data.h>
#include <api/helpers/request_helpers_fwd.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/resource_fwd.h>
#include <nx/vms/event/event_fwd.h>

struct NX_VMS_COMMON_API QnGetBookmarksRequestData: public QnMultiserverRequestData
{
    virtual void loadFromParams(
        QnResourcePool* resourcePool, const nx::network::rest::Params& params) override;
    virtual nx::network::rest::Params toParams() const override;
    virtual bool isValid() const override;

    QnCameraBookmarkSearchFilter filter;
};

struct NX_VMS_COMMON_API QnGetBookmarkTagsRequestData: public QnMultiserverRequestData
{
    explicit QnGetBookmarkTagsRequestData(int limit = unlimited());

    virtual void loadFromParams(
        QnResourcePool* resourcePool, const nx::network::rest::Params& params) override;
    virtual nx::network::rest::Params toParams() const override;
    virtual bool isValid() const override;

    static int unlimited();

    int limit = unlimited();
};

struct NX_VMS_COMMON_API QnUpdateBookmarkRequestData: public QnMultiserverRequestData
{
    QnUpdateBookmarkRequestData();
    QnUpdateBookmarkRequestData(const QnCameraBookmark& bookmark,
        const nx::Uuid& eventRuleId = nx::Uuid());

    virtual void loadFromParams(
        QnResourcePool* resourcePool, const nx::network::rest::Params& params) override;
    virtual nx::network::rest::Params toParams() const override;
    virtual bool isValid() const override;

    QnCameraBookmark bookmark;
    nx::Uuid eventRuleId;
};

struct NX_VMS_COMMON_API QnDeleteBookmarkRequestData: public QnMultiserverRequestData
{
    QnDeleteBookmarkRequestData();
    QnDeleteBookmarkRequestData(const nx::Uuid& bookmarkId);

    virtual void loadFromParams(
        QnResourcePool* resourcePool, const nx::network::rest::Params& params) override;
    virtual nx::network::rest::Params toParams() const override;
    virtual bool isValid() const override;

    nx::Uuid bookmarkId;
};
