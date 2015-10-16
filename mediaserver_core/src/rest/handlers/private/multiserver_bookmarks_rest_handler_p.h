#pragma once

#include <api/api_fwd.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>

enum class QnBookmarkOperation {
    Get,
    Add,
    Update,
    Delete
};

class QnMultiserverBookmarksRestHandlerPrivate {
public:
    static QString urlPath;
    static QnCameraBookmarkList getBookmarks(const QnGetBookmarksRequestData& request);
    static bool addBookmark(const QnUpdateBookmarkRequestData &request);
    static bool updateBookmark(const QnUpdateBookmarkRequestData &request);
    static bool deleteBookmark(const QnDeleteBookmarkRequestData &request);

    static QnBookmarkOperation getOperation(const QString &path);

private:
    struct RemoteRequestContext;
    struct GetBookmarksContext;
    struct UpdateBookmarkContext;
    struct DeleteBookmarkContext;

    static void waitForDone(RemoteRequestContext* ctx);
    static void getBookmarksRemoteAsync(MultiServerCameraBookmarkList& outputData, const QnMediaServerResourcePtr &server, GetBookmarksContext* ctx);
    static void getBookmarksLocal(MultiServerCameraBookmarkList& outputData, GetBookmarksContext* ctx);
    static void updateBookmarkRemoteAsync(const QnMediaServerResourcePtr &server, UpdateBookmarkContext* ctx);
    static void deleteBookmarkRemoteAsync(const QnMediaServerResourcePtr &server, DeleteBookmarkContext* ctx);
};
