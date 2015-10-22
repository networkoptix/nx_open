#pragma once

#include <core/resource/camera_bookmark_fwd.h>
#include <rest/helpers/request_helpers_fwd.h>
#include <api/helpers/bookmark_request_data.h>

class QnBookmarksRequestHelper
{
public:
    static QnCameraBookmarkList loadBookmarks(const QnGetBookmarksRequestData& request);
    static QnCameraBookmarkTagList loadTags(const QnGetBookmarkTagsRequestData &request);
};

struct QnMultiserverRequestContext {

    QnMultiserverRequestContext();

    QnMutex mutex;
    QnWaitCondition waitCond;
    int requestsInProgress;
};

struct QnGetBookmarksRequestContext: public QnMultiserverRequestContext {
    QnGetBookmarksRequestContext(const QnGetBookmarksRequestData& request);

    const QnGetBookmarksRequestData request;
};

struct QnGetBookmarkTagsRequestContext: public QnMultiserverRequestContext {
    QnGetBookmarkTagsRequestContext(const QnGetBookmarkTagsRequestData& request);

    const QnGetBookmarkTagsRequestData request;
};

struct QnUpdateBookmarkRequestContext: public QnMultiserverRequestContext {
    QnUpdateBookmarkRequestContext(const QnUpdateBookmarkRequestData &request);

    const QnUpdateBookmarkRequestData request;
};

struct QnDeleteBookmarkRequestContext: public QnMultiserverRequestContext {
    QnDeleteBookmarkRequestContext(const QnDeleteBookmarkRequestData &request);

    const QnDeleteBookmarkRequestData request;
};