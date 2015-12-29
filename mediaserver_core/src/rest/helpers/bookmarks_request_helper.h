#pragma once

#include <rest/helpers/request_context.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <api/helpers/bookmark_request_data.h>

class QnBookmarksRequestHelper
{
public:
    static QnCameraBookmarkList loadBookmarks(const QnGetBookmarksRequestData& request);
    static QnCameraBookmarkTagList loadTags(const QnGetBookmarkTagsRequestData &request);
};

typedef QnMultiserverRequestContext<QnGetBookmarksRequestData> QnGetBookmarksRequestContext;
typedef QnMultiserverRequestContext<QnGetBookmarkTagsRequestData> QnGetBookmarkTagsRequestContext;
typedef QnMultiserverRequestContext<QnUpdateBookmarkRequestData> QnUpdateBookmarkRequestContext;
typedef QnMultiserverRequestContext<QnDeleteBookmarkRequestData> QnDeleteBookmarkRequestContext;
