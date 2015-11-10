#include "multiserver_bookmarks_rest_handler.h"

#include <api/helpers/bookmark_request_data.h>
 
#include <core/resource/camera_bookmark.h>
 
#include <rest/handlers/private/multiserver_bookmarks_rest_handler_p.h>

QnMultiserverBookmarksRestHandler::QnMultiserverBookmarksRestHandler(const QString& path): QnFusionRestHandler()
{
    QnMultiserverBookmarksRestHandlerPrivate::urlPath = path;
}

int QnMultiserverBookmarksRestHandler::executeGet(const QString& path, 
                                                  const QnRequestParamList& params, 
                                                  QByteArray& result, 
                                                  QByteArray& contentType, 
                                                  const QnRestConnectionProcessor *processor)
{
    Q_UNUSED(processor)

    QString action = extractAction(path);
    QnBookmarkOperation op = QnMultiserverBookmarksRestHandlerPrivate::getOperation(action);

    switch (op) {
    case QnBookmarkOperation::Add:
    case QnBookmarkOperation::Update:
        {
            auto request = QnMultiserverRequestData::fromParams<QnUpdateBookmarkRequestData>(params);
            if (!request.isValid())
                return nx_http::StatusCode::badRequest;
            
            bool result = op == QnBookmarkOperation::Add
                ? QnMultiserverBookmarksRestHandlerPrivate::addBookmark(request)
                : QnMultiserverBookmarksRestHandlerPrivate::updateBookmark(request);

            return result
                ? nx_http::StatusCode::ok
                : nx_http::StatusCode::internalServerError;
        }

    case QnBookmarkOperation::Delete:
        {
            auto request = QnDeleteBookmarkRequestData::fromParams<QnDeleteBookmarkRequestData>(params);
            if (!request.isValid())
                return nx_http::StatusCode::badRequest;

            return QnMultiserverBookmarksRestHandlerPrivate::deleteBookmark(request)
                ? nx_http::StatusCode::ok
                : nx_http::StatusCode::internalServerError;
        }
    case QnBookmarkOperation::GetTags:
        {
            auto request = QnGetBookmarkTagsRequestData::fromParams<QnGetBookmarkTagsRequestData>(params);
            if (!request.isValid())
                return nx_http::StatusCode::badRequest;
            QnCameraBookmarkTagList outputData = QnMultiserverBookmarksRestHandlerPrivate::getBookmarkTags(request);
            QnFusionRestHandlerDetail::serialize(outputData, params, result, contentType, request.format);
            return nx_http::StatusCode::ok;
        }
    default:
        {
            QnGetBookmarksRequestData request = QnMultiserverRequestData::fromParams<QnGetBookmarksRequestData>(params);
            if (!request.isValid())
                return nx_http::StatusCode::badRequest;
            QnCameraBookmarkList outputData = QnMultiserverBookmarksRestHandlerPrivate::getBookmarks(request);
            QnFusionRestHandlerDetail::serialize(outputData, params, result, contentType, request.format);
            return nx_http::StatusCode::ok;
        }
    }

    
}
