#include "camera_bookmarks_rest_handler.h"

#include <api/app_server_connection.h>

#include <core/resource/camera_bookmark_fwd.h>
#include <database/server_db.h>

#include <network/tcp_connection_priv.h>
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
    return nx_http::StatusCode::notFound;
}

int QnCameraBookmarksRestHandler::addCameraBookmarkAction(const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result) { 
    Q_UNUSED(params)

    QnCameraBookmark bookmark;
    if (!QJson::deserialize(body, &bookmark))
        return nx_http::StatusCode::invalidParameter;

    if (bookmark.cameraId.isEmpty())
        return nx_http::StatusCode::invalidParameter;

    if (!qnServerDb->addBookmark(bookmark))
        return nx_http::StatusCode::invalidParameter;

    result.setReply(bookmark);
    return nx_http::StatusCode::ok;
}

int QnCameraBookmarksRestHandler::updateCameraBookmarkAction(const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result) { 
    Q_UNUSED(params)

    QnCameraBookmark bookmark;
    if (!QJson::deserialize(body, &bookmark))
        return nx_http::StatusCode::invalidParameter;

    if (bookmark.cameraId.isEmpty())
        return nx_http::StatusCode::invalidParameter;

    if (!qnServerDb->updateBookmark(bookmark))
        return nx_http::StatusCode::invalidParameter;

    result.setReply(bookmark);
    return nx_http::StatusCode::ok;
}

int QnCameraBookmarksRestHandler::deleteCameraBookmarkAction(const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result) {   
    Q_UNUSED(body)
    Q_UNUSED(result)

    QnUuid bookmarkId = QnUuid::fromStringSafe(params.value(lit("id")));
    if (bookmarkId.isNull())
        return nx_http::StatusCode::invalidParameter;

    if (!qnServerDb->deleteBookmark(bookmarkId))
        return nx_http::StatusCode::invalidParameter;

    return nx_http::StatusCode::ok;
}
