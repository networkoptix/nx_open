#pragma once

#include <api/helpers/request_helpers_fwd.h>
#include <core/resource/resource_fwd.h>
#include <core/resource/camera_bookmark_fwd.h>
#include <rest/helpers/request_helpers_fwd.h>

enum class QnBookmarkOperation {
    Get,
    GetTags,
    Add,
    Update,
    Delete,

    Count
};

class QnMultiserverBookmarksRestHandlerPrivate {
public:
    static QString urlPath;
    static QnCameraBookmarkList getBookmarks(const QnGetBookmarksRequestData& request);
    static QnCameraBookmarkTagList getBookmarkTags(const QnGetBookmarkTagsRequestData& request);
    static bool addBookmark(const QnUpdateBookmarkRequestData &request);
    static bool updateBookmark(const QnUpdateBookmarkRequestData &request);
    static bool deleteBookmark(const QnDeleteBookmarkRequestData &request);

    static QnBookmarkOperation getOperation(const QString &path);

private:
    static void waitForDone(QnMultiserverRequestContext* ctx);
    static void getBookmarksLocal(QnMultiServerCameraBookmarkList& outputData, QnGetBookmarksRequestContext* ctx);
    static void getBookmarksRemoteAsync(QnMultiServerCameraBookmarkList& outputData, const QnMediaServerResourcePtr &server, QnGetBookmarksRequestContext* ctx);

    static void getBookmarkTagsLocal(QnMultiServerCameraBookmarkTagList& outputData, QnGetBookmarkTagsRequestContext* ctx);
    static void getBookmarkTagsRemoteAsync(QnMultiServerCameraBookmarkTagList& outputData, const QnMediaServerResourcePtr &server, QnGetBookmarkTagsRequestContext* ctx);
    
    static void updateBookmarkRemoteAsync(const QnMediaServerResourcePtr &server, QnUpdateBookmarkRequestContext* ctx);
    static void deleteBookmarkRemoteAsync(const QnMediaServerResourcePtr &server, QnDeleteBookmarkRequestContext* ctx);

    static void sendAsyncRequest(const QnMediaServerResourcePtr &server, QUrl url, QnMultiserverRequestContext *ctx);
};
