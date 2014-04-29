#include "camera_bookmarks_rest_handler.h"

#include <core/resource/camera_bookmark.h>
#include <recorder/storage_manager.h>

#include <utils/network/tcp_connection_priv.h>

int QnCameraBookmarksRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result) {
    return CODE_NOT_FOUND;
}

int QnCameraBookmarksRestHandler::executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result) {
    QString action = extractAction(path);
    if (action == "add")
        return addCameraBookmarkAction(params, body, result);
    else
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


