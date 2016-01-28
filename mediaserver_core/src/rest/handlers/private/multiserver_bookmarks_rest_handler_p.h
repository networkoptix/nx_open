#pragma once

#include <api/helpers/request_helpers_fwd.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <rest/helpers/request_context.h>
#include <rest/server/rest_connection_processor.h>

enum class QnBookmarkOperation
{
    Get,
    GetTags,
    Add,
    Update,
    Delete,

    Count
};

typedef QnMultiserverRequestContext<QnGetBookmarksRequestData> QnGetBookmarksRequestContext;
typedef QnMultiserverRequestContext<QnGetBookmarkTagsRequestData> QnGetBookmarkTagsRequestContext;
typedef QnMultiserverRequestContext<QnUpdateBookmarkRequestData> QnUpdateBookmarkRequestContext;
typedef QnMultiserverRequestContext<QnDeleteBookmarkRequestData> QnDeleteBookmarkRequestContext;

class QnMultiserverBookmarksRestHandlerPrivate
{
public:
    static QString urlPath;
    static QnCameraBookmarkList getBookmarks(QnGetBookmarksRequestContext& context);
    static QnCameraBookmarkTagList getBookmarkTags(QnGetBookmarkTagsRequestContext& context);
    static bool addBookmark(QnUpdateBookmarkRequestContext &context);
    static bool updateBookmark(QnUpdateBookmarkRequestContext &context);
    static bool deleteBookmark(QnDeleteBookmarkRequestContext &context);

    static QnBookmarkOperation getOperation(const QString &path);
};
