#include "multiserver_bookmarks_rest_handler.h"

#include <api/helpers/bookmark_request_data.h>

#include <core/resource/camera_bookmark.h>

#include <network/tcp_listener.h>
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
    QString action = extractAction(path);
    QnBookmarkOperation op = QnMultiserverBookmarksRestHandlerPrivate::getOperation(action);

    const auto ownerPort = processor->owner()->getPort();
    switch (op) {
    case QnBookmarkOperation::Add:
    case QnBookmarkOperation::Update:
        {
            QnRestResult execResult;
            auto request = QnMultiserverRequestData::fromParams<QnUpdateBookmarkRequestData>(params);
            if (!request.isValid()) {
                execResult.error = QnRestResult::MissingParameter;
                execResult.errorString = lit("missing parameter(s)");
                QnFusionRestHandlerDetail::serialize(execResult, result, contentType, request.format, request.extraFormatting);
                return nx_http::StatusCode::badRequest;
            }

            QnUpdateBookmarkRequestContext context(request, ownerPort);
            bool ok = op == QnBookmarkOperation::Add
                ? QnMultiserverBookmarksRestHandlerPrivate::addBookmark(context)
                : QnMultiserverBookmarksRestHandlerPrivate::updateBookmark(context);
            if (!ok) {
                execResult.error = QnRestResult::CantProcessRequest;
                execResult.errorString = lit("Can't %1 bookmark").arg(op == QnBookmarkOperation::Add ? lit("add") : lit("update"));
            }
            QnFusionRestHandlerDetail::serialize(execResult, result, contentType, request.format, request.extraFormatting);
            return ok
                ? nx_http::StatusCode::ok
                : nx_http::StatusCode::internalServerError;
        }

    case QnBookmarkOperation::Delete:
        {
            QnRestResult execResult;
            auto request = QnDeleteBookmarkRequestData::fromParams<QnDeleteBookmarkRequestData>(params);
            if (!request.isValid()) {
                execResult.error = QnRestResult::MissingParameter;
                execResult.errorString = lit("missing parameter(s)");
                QnFusionRestHandlerDetail::serialize(execResult, result, contentType, request.format, request.extraFormatting);
                return nx_http::StatusCode::badRequest;
            }

            QnDeleteBookmarkRequestContext context(request, ownerPort);
            bool ok = QnMultiserverBookmarksRestHandlerPrivate::deleteBookmark(context);
            if (!ok) {
                execResult.error = QnRestResult::CantProcessRequest;
                execResult.errorString = lit("Can't delete bookmark");
            }
            return ok
                ? nx_http::StatusCode::ok
                : nx_http::StatusCode::internalServerError;
        }
    case QnBookmarkOperation::GetTags:
        {
            auto request = QnGetBookmarkTagsRequestData::fromParams<QnGetBookmarkTagsRequestData>(params);
            if (!request.isValid())
                return nx_http::StatusCode::badRequest;

            QnGetBookmarkTagsRequestContext context(request, ownerPort);
            QnCameraBookmarkTagList outputData = QnMultiserverBookmarksRestHandlerPrivate::getBookmarkTags(context);
            QnFusionRestHandlerDetail::serialize(outputData, result, contentType, request.format, request.extraFormatting);
            return nx_http::StatusCode::ok;
        }
    default:
        {
            QnGetBookmarksRequestData request = QnMultiserverRequestData::fromParams<QnGetBookmarksRequestData>(params);
            if (!request.isValid())
                return nx_http::StatusCode::badRequest;

            QnGetBookmarksRequestContext context(request, ownerPort);
            QnCameraBookmarkList outputData = QnMultiserverBookmarksRestHandlerPrivate::getBookmarks(context);
            QnFusionRestHandlerDetail::serialize(outputData, result, contentType, request.format, request.extraFormatting);
            return nx_http::StatusCode::ok;
        }
    }


}
