#include "camera_bookmarks_rest_handler.h"

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
    return CODE_OK;
}
