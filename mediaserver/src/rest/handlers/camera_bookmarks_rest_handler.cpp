#include "camera_bookmarks_rest_handler.h"

#include <core/resource/camera_bookmark_fwd.h>
#include <recorder/storage_manager.h>

#include <utils/network/tcp_connection_priv.h>

int QnCameraBookmarksRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result) {
    QString action = extractAction(path);
    if (action == "get")
        return getCameraBookmarkAction(params, result);
    return CODE_NOT_FOUND;
}

int QnCameraBookmarksRestHandler::executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result) {
    QString action = extractAction(path);
    if (action == "add")
        return addCameraBookmarkAction(params, body, result);
    return CODE_NOT_FOUND;
}

QString QnCameraBookmarksRestHandler::description() const {
    return QString();
}

int QnCameraBookmarksRestHandler::addCameraBookmarkAction(const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result) { 
    QString id = params.value("id");
    QnCameraBookmark bookmark;
    if (id.isEmpty() || !QJson::deserialize(body, &bookmark))
        return CODE_INVALID_PARAMETER;

    if (!qnStorageMan->addBookmark(id.toUtf8(), bookmark)) 
        return CODE_INVALID_PARAMETER;

    result.setReply(bookmark);
    return CODE_OK;
}

int QnCameraBookmarksRestHandler::getCameraBookmarkAction(const QnRequestParams & params, QnJsonRestResult & result) {
    QString id = params.value("id");
    QnCameraBookmarkSearchFilter filter;
    bool ok;
    if (qint64 value = params["minStartTimeMs"].toLongLong(&ok) && ok)
        filter.minDurationMs = value;
    if (qint64 value = params["minStartTimeMs"].toLongLong(&ok) && ok)
        filter.minStartTimeMs = value;
    if (qint64 value = params["maxStartTimeMs"].toLongLong(&ok) && ok)
        filter.maxStartTimeMs = value;

    QnCameraBookmarkList bookmarks;
    if (!qnStorageMan->getBookmarks(id.toUtf8(), filter, bookmarks))
        return CODE_INVALID_PARAMETER;

    result.setReply(bookmarks.first()); //TODO: #GDM WTF? What else should I do to deserialize QnCameraBookmarkList ???
    return CODE_OK;
}


