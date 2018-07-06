#include "multiserver_bookmarks_rest_handler.h"

#include <api/helpers/bookmark_request_data.h>

#include <core/resource/camera_bookmark.h>

#include <network/tcp_listener.h>

#include <rest/helpers/permissions_helper.h>
#include <rest/handlers/private/multiserver_bookmarks_rest_handler_p.h>
#include <core/resource_access/resource_access_manager.h>
#include <common/common_module.h>

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
    const auto& commonModule = processor->commonModule();

    Qn::GlobalPermission requiredPermission = Qn::GlobalViewBookmarksPermission;
    if (isModifyingOperation(op))
    {
        if (QnPermissionsHelper::isSafeMode(commonModule))
        {
            const QnBaseMultiserverRequestData request(params);
            return QnPermissionsHelper::safeModeError(
                *outBody, *outContentType, request.format, request.extraFormatting);
        }
        requiredPermission = Qn::GlobalManageBookmarksPermission;
    }
    if (!commonModule->resourceAccessManager()->hasGlobalPermission(
        processor->accessRights(), requiredPermission))
    {
        const QnBaseMultiserverRequestData request(params);
        return QnPermissionsHelper::permissionsError(
            *outBody, *outContentType, request.format, request.extraFormatting);
    }

    return nx::network::http::StatusCode::ok;
}

static const QString kInvalidParamsMessage =
    lit("Missing parameter(s) or invalid parameter values");

static int performAddOrUpdate(
    QnCommonModule* commonModule,
    QnBookmarkOperation op,
    int ownerPort,
    const QnRequestParamList& params,
    const QnUuid& authoredUserId,
    QByteArray* outBody,
    QByteArray* outContentType)
{
    auto request =
        QnMultiserverRequestData::fromParams<QnUpdateBookmarkRequestData>(commonModule->resourcePool(), params);
    if (!request.isValid())
    {
        return QnFusionRestHandler::makeError(nx::network::http::StatusCode::badRequest,
            kInvalidParamsMessage,
            outBody, outContentType, request.format, request.extraFormatting,
            QnRestResult::MissingParameter);
    }

    QnUpdateBookmarkRequestContext context(request, ownerPort);
    bool ok = (op == QnBookmarkOperation::Add || op == QnBookmarkOperation::Acknowledge)
        ? QnMultiserverBookmarksRestHandlerPrivate::addBookmark(commonModule, context, authoredUserId)
        : QnMultiserverBookmarksRestHandlerPrivate::updateBookmark(commonModule, context);
    if (!ok)
    {
        return QnFusionRestHandler::makeError(nx::network::http::StatusCode::badRequest,
            lit("Can't %1 bookmark").arg(
                (op == QnBookmarkOperation::Add) ? lit("add") : lit("update")),
            outBody, outContentType, request.format, request.extraFormatting,
            QnRestResult::CantProcessRequest);
    }
    QnRestResult execResult;
    QnFusionRestHandlerDetail::serialize(
        execResult, *outBody, *outContentType, request.format, request.extraFormatting);
    return nx::network::http::StatusCode::ok;
}

static int performDelete(
    QnCommonModule* commonModule,
    int ownerPort,
    const QnRequestParamList& params,
    QByteArray* outBody,
    QByteArray* outContentType)
{
    auto request =
        QnDeleteBookmarkRequestData::fromParams<QnDeleteBookmarkRequestData>(commonModule->resourcePool(), params);
    if (!request.isValid())
    {
        return QnFusionRestHandler::makeError(nx::network::http::StatusCode::badRequest,
            kInvalidParamsMessage,
            outBody, outContentType, request.format, request.extraFormatting,
            QnRestResult::MissingParameter);
    }

    QnDeleteBookmarkRequestContext context(request, ownerPort);
    if (!QnMultiserverBookmarksRestHandlerPrivate::deleteBookmark(commonModule, context))
    {
        return QnFusionRestHandler::makeError(nx::network::http::StatusCode::internalServerError,
            lit("Can't delete bookmark"),
            outBody, outContentType, request.format, request.extraFormatting,
            QnRestResult::CantProcessRequest);
    }
    return nx::network::http::StatusCode::ok;
}

static int performGetTags(
    QnCommonModule* commonModule,
    int ownerPort,
    const QnRequestParamList& params,
    QByteArray* outBody,
    QByteArray* outContentType)
{
    auto request =
        QnGetBookmarkTagsRequestData::fromParams<QnGetBookmarkTagsRequestData>(commonModule->resourcePool(), params);
    if (!request.isValid())
    {
        return QnFusionRestHandler::makeError(nx::network::http::StatusCode::badRequest,
            kInvalidParamsMessage,
            outBody, outContentType, request.format, request.extraFormatting,
            QnRestResult::MissingParameter);
    }

    QnGetBookmarkTagsRequestContext context(request, ownerPort);
    QnCameraBookmarkTagList outputData =
        QnMultiserverBookmarksRestHandlerPrivate::getBookmarkTags(commonModule, context);
    QnFusionRestHandlerDetail::serialize(
        outputData, *outBody, *outContentType, request.format, request.extraFormatting);
    return nx::network::http::StatusCode::ok;
}

static int performGet(
    QnCommonModule* commonModule,
    int ownerPort,
    const QnRequestParamList& params,
    QByteArray* outBody,
    QByteArray* outContentType)
{
    QnGetBookmarksRequestData request =
        QnMultiserverRequestData::fromParams<QnGetBookmarksRequestData>(commonModule->resourcePool(), params);
    if (!request.isValid())
    {
        return QnFusionRestHandler::makeError(nx::network::http::StatusCode::badRequest,
            kInvalidParamsMessage,
            outBody, outContentType, request.format, request.extraFormatting,
            QnRestResult::MissingParameter);
    }

    QnGetBookmarksRequestContext context(request, ownerPort);
    QnCameraBookmarkList outputData =
        QnMultiserverBookmarksRestHandlerPrivate::getBookmarks(commonModule, context);
    QnFusionRestHandlerDetail::serialize(
        outputData, *outBody, *outContentType, request.format, request.extraFormatting);
    return nx::network::http::StatusCode::ok;
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
    const auto& commonModule = processor->commonModule();
    QString action = extractAction(path);
    QnBookmarkOperation op = QnMultiserverBookmarksRestHandlerPrivate::getOperation(action);

    const int status = checkPermissions(op, params, &result, &contentType, processor);
    if (status != nx::network::http::StatusCode::ok)
        return status;

    const auto ownerPort = processor->owner()->getPort();

    switch (op)
    {
        case QnBookmarkOperation::Add:
        case QnBookmarkOperation::Update:
        case QnBookmarkOperation::Acknowledge:
        {
            const auto authoredUserId = processor->accessRights().userId;
            return performAddOrUpdate(commonModule, op, ownerPort, params, authoredUserId,
                &result, &contentType);
        }

        case QnBookmarkOperation::Delete:
            return performDelete(commonModule, ownerPort, params, &result, &contentType);

        case QnBookmarkOperation::GetTags:
            return performGetTags(commonModule, ownerPort, params, &result, &contentType);

        default:
            return performGet(commonModule, ownerPort, params, &result, &contentType);
    }
}
