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
    static QnCameraBookmarkList getBookmarks(QnCommonModule* commonModule, QnGetBookmarksRequestContext& context);
    static QnCameraBookmarkTagList getBookmarkTags(QnCommonModule* commonModule, QnGetBookmarkTagsRequestContext& context);
    static bool addBookmark(QnCommonModule* commonModule, QnUpdateBookmarkRequestContext &context);
    static bool updateBookmark(QnCommonModule* commonModule, QnUpdateBookmarkRequestContext &context);
    static bool deleteBookmark(QnCommonModule* commonModule, QnDeleteBookmarkRequestContext &context);

    static QnBookmarkOperation getOperation(const QString &path);
};
