#include "camera_bookmarks_rest_handler.h"

#include <core/resource/camera_bookmark_fwd.h>
#include <recorder/storage_manager.h>

#include <utils/network/tcp_connection_priv.h>
#include <utils/serialization/json_functions.h>

int QnCameraBookmarksRestHandler::executeGet(const QString &path, const QnRequestParams &params, QnJsonRestResult &result, const QnRestConnectionProcessor*) 
{
    QString action = extractAction(path);
    if (action == "get")
        return getCameraBookmarksAction(params, result);
    return CODE_NOT_FOUND;
}

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

    if (!qnStorageMan->addBookmark(id.toUtf8(), bookmark)) 
        return CODE_INVALID_PARAMETER;

    result.setReply(bookmark);
    return CODE_OK;
}


int QnCameraBookmarksRestHandler::updateCameraBookmarkAction(const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result) {
    QString id = params.value("id");
    QnCameraBookmark bookmark;
    if (id.isEmpty() || !QJson::deserialize(body, &bookmark))
        return CODE_INVALID_PARAMETER;

    if (!qnStorageMan->updateBookmark(id.toUtf8(), bookmark)) 
        return CODE_INVALID_PARAMETER;

    result.setReply(bookmark);
    return CODE_OK;
}

int QnCameraBookmarksRestHandler::deleteCameraBookmarkAction(const QnRequestParams &params, const QByteArray &body, QnJsonRestResult &result) {
    QString id = params.value("id");
    QnCameraBookmark bookmark;
    if (id.isEmpty() || !QJson::deserialize(body, &bookmark))
        return CODE_INVALID_PARAMETER;

    if (!qnStorageMan->deleteBookmark(id.toUtf8(), bookmark))
        return CODE_INVALID_PARAMETER;

    result.setReply(bookmark);
    return CODE_OK;
}


int QnCameraBookmarksRestHandler::getCameraBookmarksAction(const QnRequestParams & params, QnJsonRestResult & result) {
    //TODO: #GDM #Bookmarks check time periods existence via qnStorageMan

    QString id = params.value("id");
    QnCameraBookmarkSearchFilter filter;
    bool ok;
    if (qint64 value = params["minDurationMs"].toLongLong(&ok))
        if (ok) filter.minDurationMs = value;
    if (qint64 value = params["minStartTimeMs"].toLongLong(&ok))
        if (ok) filter.minStartTimeMs = value;
    if (qint64 value = params["maxStartTimeMs"].toLongLong(&ok))
        if (ok) filter.maxStartTimeMs = value;
    filter.text = params["text"];

    QnTimePeriod period(filter.minStartTimeMs, filter.maxStartTimeMs - filter.minStartTimeMs);
    qDebug() << "bookmarks requested with resolution" << filter.minDurationMs << "for" << period;   //TODO: #GDM #Bookmarks remove when profiling will be finished

    QnCameraBookmarkList bookmarks;
    if (!qnStorageMan->getBookmarks(id.toUtf8(), filter, bookmarks))
        return CODE_INVALID_PARAMETER;

    result.setReply(bookmarks);
    return CODE_OK;
}


