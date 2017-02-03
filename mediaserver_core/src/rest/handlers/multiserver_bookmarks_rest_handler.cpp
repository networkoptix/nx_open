#include "multiserver_bookmarks_rest_handler.h"

#include <api/helpers/bookmark_request_data.h>

#include <core/resource/camera_bookmark.h>

#include <network/tcp_listener.h>

#include <rest/helpers/permissions_helper.h>
#include <rest/handlers/private/multiserver_bookmarks_rest_handler_p.h>
#include <core/resource_access/resource_access_manager.h>

namespace {

/**
 * Check if operation modifies the database.
 */
bool isModifyingOperation(QnBookmarkOperation op)
{
    switch (op)
    {
        case QnBookmarkOperation::Add:
        case QnBookmarkOperation::Update:
        case QnBookmarkOperation::Delete:
            return true;
        default:
            break;
    }
    return false;
}

static int checkPermissions(
    QnBookmarkOperation op,
    const QnRequestParamList& params,
    QByteArray* outBody,
    QByteArray* outContentType,
    const QnRestConnectionProcessor* processor)
{
    Qn::GlobalPermission requiredPermission = Qn::GlobalViewBookmarksPermission;
    if (isModifyingOperation(op))
    {
        if (QnPermissionsHelper::isSafeMode())
        {
            const QnMultiserverRequestData request(params);
            return QnPermissionsHelper::safeModeError(
                *outBody, *outContentType, request.format, request.extraFormatting);
        }
        requiredPermission = Qn::GlobalManageBookmarksPermission;
    }
    if (!qnResourceAccessManager->hasGlobalPermission(
        processor->accessRights(), requiredPermission))
    {
        const QnMultiserverRequestData request(params);
        return QnPermissionsHelper::permissionsError(
            *outBody, *outContentType, request.format, request.extraFormatting);
    }

    return nx_http::StatusCode::ok;
}

static const QString kInvalidParamsMessage =
    lit("Missing parameter(s) or invalid parameter values");

static int performAddOrUpdate(
    QnBookmarkOperation op,
    int ownerPort,
    const QnRequestParamList& params,
    QByteArray* outBody,
    QByteArray* outContentType)
{
    auto request =
        QnMultiserverRequestData::fromParams<QnUpdateBookmarkRequestData>(params);
    if (!request.isValid())
    {
        return QnFusionRestHandler::makeError(nx_http::StatusCode::badRequest,
            kInvalidParamsMessage,
            outBody, outContentType, request.format, request.extraFormatting,
            QnRestResult::MissingParameter);
    }

    QnUpdateBookmarkRequestContext context(request, ownerPort);
    bool ok = (op == QnBookmarkOperation::Add)
        ? QnMultiserverBookmarksRestHandlerPrivate::addBookmark(context)
        : QnMultiserverBookmarksRestHandlerPrivate::updateBookmark(context);
    if (!ok)
    {
        return QnFusionRestHandler::makeError(nx_http::StatusCode::badRequest,
            lit("Can't %1 bookmark").arg(
                (op == QnBookmarkOperation::Add) ? lit("add") : lit("update")),
            outBody, outContentType, request.format, request.extraFormatting,
            QnRestResult::CantProcessRequest);
    }
    QnRestResult execResult;
    QnFusionRestHandlerDetail::serialize(
        execResult, *outBody, *outContentType, request.format, request.extraFormatting);
    return nx_http::StatusCode::ok;
}

static int performDelete(
    int ownerPort,
    const QnRequestParamList& params,
    QByteArray* outBody,
    QByteArray* outContentType)
{
    auto request =
        QnDeleteBookmarkRequestData::fromParams<QnDeleteBookmarkRequestData>(params);
    if (!request.isValid())
    {
        return QnFusionRestHandler::makeError(nx_http::StatusCode::badRequest,
            kInvalidParamsMessage,
            outBody, outContentType, request.format, request.extraFormatting,
            QnRestResult::MissingParameter);
    }

    QnDeleteBookmarkRequestContext context(request, ownerPort);
    if (!QnMultiserverBookmarksRestHandlerPrivate::deleteBookmark(context))
    {
        return QnFusionRestHandler::makeError(nx_http::StatusCode::internalServerError,
            lit("Can't delete bookmark"),
            outBody, outContentType, request.format, request.extraFormatting,
            QnRestResult::CantProcessRequest);
    }
    return nx_http::StatusCode::ok;
}

static int performGetTags(
    int ownerPort,
    const QnRequestParamList& params,
    QByteArray* outBody,
    QByteArray* outContentType)
{
    auto request =
        QnGetBookmarkTagsRequestData::fromParams<QnGetBookmarkTagsRequestData>(params);
    if (!request.isValid())
    {
        return QnFusionRestHandler::makeError(nx_http::StatusCode::badRequest,
            kInvalidParamsMessage,
            outBody, outContentType, request.format, request.extraFormatting,
            QnRestResult::MissingParameter);
    }

    QnGetBookmarkTagsRequestContext context(request, ownerPort);
    QnCameraBookmarkTagList outputData =
        QnMultiserverBookmarksRestHandlerPrivate::getBookmarkTags(context);
    QnFusionRestHandlerDetail::serialize(
        outputData, *outBody, *outContentType, request.format, request.extraFormatting);
    return nx_http::StatusCode::ok;
}

static int performGet(
    int ownerPort,
    const QnRequestParamList& params,
    QByteArray* outBody,
    QByteArray* outContentType)
{
    QnGetBookmarksRequestData request =
        QnMultiserverRequestData::fromParams<QnGetBookmarksRequestData>(params);
    if (!request.isValid())
    {
        return QnFusionRestHandler::makeError(nx_http::StatusCode::badRequest,
            kInvalidParamsMessage,
            outBody, outContentType, request.format, request.extraFormatting,
            QnRestResult::MissingParameter);
    }

    QnGetBookmarksRequestContext context(request, ownerPort);
    QnCameraBookmarkList outputData =
        QnMultiserverBookmarksRestHandlerPrivate::getBookmarks(context);
    QnFusionRestHandlerDetail::serialize(
        outputData, *outBody, *outContentType, request.format, request.extraFormatting);
    return nx_http::StatusCode::ok;
}

} // namespace

QnMultiserverBookmarksRestHandler::QnMultiserverBookmarksRestHandler(const QString& path):
    QnFusionRestHandler()
{
    QnMultiserverBookmarksRestHandlerPrivate::urlPath = path;
}

int QnMultiserverBookmarksRestHandler::executeGet(
    const QString& path,
    const QnRequestParamList& params,
    QByteArray& result,
    QByteArray& contentType,
    const QnRestConnectionProcessor* processor)
{
    QString action = extractAction(path);
    QnBookmarkOperation op = QnMultiserverBookmarksRestHandlerPrivate::getOperation(action);

    const int status = checkPermissions(op, params, &result, &contentType, processor);
    if (status != nx_http::StatusCode::ok)
        return status;

    const auto ownerPort = processor->owner()->getPort();

    switch (op)
    {
        case QnBookmarkOperation::Add:
        case QnBookmarkOperation::Update:
            return performAddOrUpdate(op, ownerPort, params, &result, &contentType);

        case QnBookmarkOperation::Delete:
            return performDelete(ownerPort, params, &result, &contentType);

        case QnBookmarkOperation::GetTags:
            return performGetTags(ownerPort, params, &result, &contentType);

        default:
            return performGet(ownerPort, params, &result, &contentType);
    }
}
