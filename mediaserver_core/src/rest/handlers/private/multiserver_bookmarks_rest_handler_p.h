#pragma once

#include <api/helpers/request_helpers_fwd.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <rest/helpers/request_context.h>
#include <rest/server/rest_connection_processor.h>

class QnMediaServerModule;

enum class QnBookmarkOperation
{
    Get,
    GetTags,
    Add,
    Acknowledge,
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
    static QnCameraBookmarkList getBookmarks(QnMediaServerModule* serverModule, QnGetBookmarksRequestContext& context);
    static QnCameraBookmarkTagList getBookmarkTags(QnMediaServerModule* serverModule, QnGetBookmarkTagsRequestContext& context);
    static bool addBookmark(
        QnMediaServerModule* serverModule,
        QnUpdateBookmarkRequestContext &context,
        const QnUuid& authorityUser);
    static bool updateBookmark(QnMediaServerModule* serverModule, QnUpdateBookmarkRequestContext &context);
    static bool deleteBookmark(QnMediaServerModule* serverModule, QnDeleteBookmarkRequestContext &context);

    static QnBookmarkOperation getOperation(const QString &path);
};
