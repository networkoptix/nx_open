#include "camera_bookmarks_rest_handler.h"

#include <core/resource/camera_bookmark_fwd.h>
#include <recorder/storage_manager.h>

#include <utils/network/tcp_connection_priv.h>
#include <utils/serialization/json_functions.h>

int QnCameraBookmarksRestHandler::executePost(const QString &path, const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result, const QnRestConnectionProcessor*) 
{
    QString action = extractAction(path);
    if (action == "add")
        return addCameraBookmarkAction(params, body, result);
    if (action == "update")
        return updateCameraBookmarkAction(params, body, result);
    if (action == "delete")
        return deleteCameraBookmarkAction(params, body, result);
    return CODE_NOT_FOUND;
}

int QnCameraBookmarksRestHandler::addCameraBookmarkAction(const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result) { 
    QString id = params.value("id");
    QnCameraBookmark bookmark;
    if (id.isEmpty() || !QJson::deserialize(body, &bookmark))
        return CODE_INVALID_PARAMETER;

    if (!qnNormalStorageMan->addBookmark(id.toUtf8(), bookmark) && !qnBackupStorageMan->addBookmark(id.toUtf8(), bookmark)) 
        return CODE_INVALID_PARAMETER;

    result.setReply(bookmark);
    return CODE_OK;
}


int QnCameraBookmarksRestHandler::updateCameraBookmarkAction(const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result) {
    QString id = params.value("id");
    QnCameraBookmark bookmark;
    if (id.isEmpty() || !QJson::deserialize(body, &bookmark))
        return CODE_INVALID_PARAMETER;

    if (!qnNormalStorageMan->updateBookmark(id.toUtf8(), bookmark) && !qnBackupStorageMan->updateBookmark(id.toUtf8(), bookmark)) 
        return CODE_INVALID_PARAMETER;

    result.setReply(bookmark);
    return CODE_OK;
}

int QnCameraBookmarksRestHandler::deleteCameraBookmarkAction(const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result) {
    QString id = params.value("id");
    QnCameraBookmark bookmark;
    if (id.isEmpty() || !QJson::deserialize(body, &bookmark))
        return CODE_INVALID_PARAMETER;

    if (!qnNormalStorageMan->deleteBookmark(id.toUtf8(), bookmark) && !qnBackupStorageMan->deleteBookmark(id.toUtf8(), bookmark))
        return CODE_INVALID_PARAMETER;

    result.setReply(bookmark);
    return CODE_OK;
}
