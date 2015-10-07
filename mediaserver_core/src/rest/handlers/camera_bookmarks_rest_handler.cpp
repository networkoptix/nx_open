#include "camera_bookmarks_rest_handler.h"

#include <api/app_server_connection.h>

#include <core/resource/camera_bookmark_fwd.h>
#include <database/server_db.h>

#include <utils/network/tcp_connection_priv.h>
#include <utils/serialization/json_functions.h>

int QnCameraBookmarksRestHandler::executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result, const QnRestConnectionProcessor*) 
{
    QString action = extractAction(path);
    if (action == "add" || action == "update")
        return addOrUpdateCameraBookmarkAction(params, body, result);
    if (action == "delete")
        return deleteCameraBookmarkAction(params, body, result);
    return CODE_NOT_FOUND;
}

int QnCameraBookmarksRestHandler::addOrUpdateCameraBookmarkAction(const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result) { 
    QnCameraBookmark bookmark;
    if (!QJson::deserialize(body, &bookmark))
        return CODE_INVALID_PARAMETER;

    if (bookmark.cameraId.isEmpty())
        return CODE_INVALID_PARAMETER;

    if (!qnServerDb->addOrUpdateCameraBookmark(bookmark))
        return CODE_INVALID_PARAMETER;

    if (!bookmark.tags.isEmpty())
        QnAppServerConnectionFactory::getConnection2()->getCameraManager()->addBookmarkTags(bookmark.tags, this, [](int /*reqID*/, ec2::ErrorCode /*errorCode*/) {});

    result.setReply(bookmark);
    return CODE_OK;
}

int QnCameraBookmarksRestHandler::deleteCameraBookmarkAction(const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result) {   
    QnCameraBookmark bookmark;
    if (!QJson::deserialize(body, &bookmark))
        return CODE_INVALID_PARAMETER;

    if (!qnServerDb->deleteCameraBookmark(bookmark))
        return CODE_INVALID_PARAMETER;

    result.setReply(bookmark);
    return CODE_OK;
}
