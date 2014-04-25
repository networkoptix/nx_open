#include "camera_bookmarks_rest_handler.h"

#include <core/resource/camera_bookmark.h>
#include <recorder/storage_manager.h>

#include <utils/network/tcp_connection_priv.h>

int QnCameraBookmarksRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result) {

    QString action = extractAction(path);
    if (action == "add")
        return addCameraBookmarkAction(params, result);
    else
        return CODE_NOT_FOUND;
}

QString QnCameraBookmarksRestHandler::description() const {
    return QString();
}

int QnCameraBookmarksRestHandler::addCameraBookmarkAction(const QnRequestParams &params, QnJsonRestResult &result) {
    QString id = params.value("id");

    QnCameraBookmark bookmark;
    bookmark.startTimeMs = params.value("startTime").toLongLong();
    bookmark.durationMs = params.value("duration").toLongLong();
    if (qnStorageMan->addBookmark(id.toUtf8(), bookmark))
        return CODE_OK;
    return CODE_INVALID_PARAMETER;
}
