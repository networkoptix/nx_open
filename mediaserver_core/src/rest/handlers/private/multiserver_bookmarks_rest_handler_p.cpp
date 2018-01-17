#include "multiserver_bookmarks_rest_handler_p.h"

#include <api/helpers/multiserver_request_data.h>
#include <api/helpers/bookmark_request_data.h>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_history.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/camera_resource.h>

#include <database/server_db.h>

#include <rest/helpers/request_helpers.h>
#include <rest/server/rest_connection_processor.h>

#include <nx/fusion/model_functions.h>
#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>
#include <nx/vms/event/rule.h>
#include <nx/vms/event/rule_manager.h>
#include <nx/vms/event/actions/common_action.h>
#include <utils/common/synctime.h>

namespace
{
    static std::array<QString, static_cast<int>(QnBookmarkOperation::Count)>  operations =
    {
        QString(),
        "tags",
        "add",
        "acknowledge",
        "update",
        "delete",
    };

    nx::utils::Url getApiUrl(const QnMediaServerResourcePtr &server, QnBookmarkOperation operation) {
        nx::utils::Url apiUrl(server->getApiUrl());
        apiUrl.setPath(L'/' + QnMultiserverBookmarksRestHandlerPrivate::urlPath + L'/' + operations[static_cast<int>(operation)]);
        return apiUrl;
    }

    template<typename Context>
    void sendAsyncRequest(
        QnCommonModule* commonModule,
        const QnMediaServerResourcePtr &server,
        nx::utils::Url url,
        Context *ctx)
    {
        auto requestCompletionFunc = [ctx] (SystemError::ErrorCode osErrorCode, int statusCode, nx::network::http::BufferType msgBody ) {
            QN_UNUSED(osErrorCode, statusCode, msgBody);
            ctx->executeGuarded([ctx]()
            {
                ctx->requestProcessed();
            });
        };

        runMultiserverDownloadRequest(commonModule->router(), url, server, requestCompletionFunc, ctx);
    }

    void getBookmarksRemoteAsync(
        QnCommonModule* commonModule,
        QnMultiServerCameraBookmarkList& outputData,
        const QnMediaServerResourcePtr &server, QnGetBookmarksRequestContext* ctx)
    {
        auto requestCompletionFunc = [ctx, &outputData] (SystemError::ErrorCode osErrorCode, int statusCode, nx::network::http::BufferType msgBody )
        {
            QnCameraBookmarkList remoteData;
            bool success = false;
            if( osErrorCode == SystemError::noError && statusCode == nx::network::http::StatusCode::ok ) {
                remoteData = QnUbjson::deserialized(msgBody, remoteData, &success);
                NX_ASSERT(success, Q_FUNC_INFO, "We should receive correct answer here");
            }

            ctx->executeGuarded([ctx, success, remoteData, &outputData]()
            {
                if (success && !remoteData.empty())
                    outputData.push_back(std::move(remoteData));

                ctx->requestProcessed();
            });
        };

        nx::utils::Url apiUrl(getApiUrl(server, QnBookmarkOperation::Get));

        QnGetBookmarksRequestData modifiedRequest = ctx->request();
        modifiedRequest.makeLocal();
        apiUrl.setQuery(modifiedRequest.toUrlQuery());

        runMultiserverDownloadRequest(commonModule->router(), apiUrl, server, requestCompletionFunc, ctx);
    }

    void getBookmarksLocal(QnMultiServerCameraBookmarkList& outputData, QnGetBookmarksRequestContext* ctx)
    {
        const auto request = ctx->request();
        QnCameraBookmarkList bookmarks;
        if (qnServerDb->getBookmarks(request.cameras, request.filter, bookmarks) && !bookmarks.empty())
        {
            ctx->executeGuarded([bookmarks, &outputData]()
            {
                outputData.push_back(std::move(bookmarks));
            });
        }
    }

    void getBookmarkTagsRemoteAsync(
        QnCommonModule* commonModule,
        QnMultiServerCameraBookmarkTagList& outputData,
        const QnMediaServerResourcePtr &server,
        QnGetBookmarkTagsRequestContext* ctx)
    {
        auto requestCompletionFunc = [ctx, &outputData] (SystemError::ErrorCode osErrorCode, int statusCode, nx::network::http::BufferType msgBody )
        {
            QnCameraBookmarkTagList remoteData;
            bool success = false;
            if( osErrorCode == SystemError::noError && statusCode == nx::network::http::StatusCode::ok ) {
                remoteData = QnUbjson::deserialized(msgBody, remoteData, &success);
                NX_ASSERT(success, Q_FUNC_INFO, "We should receive correct answer here");
            }

            ctx->executeGuarded([ctx, success, remoteData, &outputData]()
            {
                if (success && !remoteData.empty())
                    outputData.push_back(std::move(remoteData));

                ctx->requestProcessed();
            });
        };

        nx::utils::Url apiUrl(getApiUrl(server, QnBookmarkOperation::GetTags));

        QnGetBookmarkTagsRequestData modifiedRequest = ctx->request();
        modifiedRequest.makeLocal();
        apiUrl.setQuery(modifiedRequest.toUrlQuery());

        runMultiserverDownloadRequest(commonModule->router(), apiUrl, server, requestCompletionFunc, ctx);
    }

    void getBookmarkTagsLocal(QnMultiServerCameraBookmarkTagList& outputData, QnGetBookmarkTagsRequestContext* ctx)
    {
        auto tags = qnServerDb->getBookmarkTags(ctx->request().limit);
        if (!tags.empty()) {
            ctx->executeGuarded([tags, &outputData]()
            {
                outputData.push_back(std::move(tags));
            });
        }
    }

    void updateBookmarkRemoteAsync(
        QnCommonModule* commonModule,
        const QnMediaServerResourcePtr &server,
        QnUpdateBookmarkRequestContext* ctx)
    {
        nx::utils::Url apiUrl(getApiUrl(server, QnBookmarkOperation::Update));

        QnUpdateBookmarkRequestData modifiedRequest = ctx->request();
        modifiedRequest.makeLocal();
        apiUrl.setQuery(modifiedRequest.toUrlQuery());

        sendAsyncRequest(commonModule, server, apiUrl, ctx);
    }

    void deleteBookmarkRemoteAsync(
        QnCommonModule* commonModule,
        const QnMediaServerResourcePtr &server,
        QnDeleteBookmarkRequestContext* ctx)
    {
        nx::utils::Url apiUrl(getApiUrl(server, QnBookmarkOperation::Delete));

        QnDeleteBookmarkRequestData modifiedRequest = ctx->request();
        modifiedRequest.makeLocal();
        apiUrl.setQuery(modifiedRequest.toUrlQuery());

        sendAsyncRequest(commonModule, server, apiUrl, ctx);
    }
}

QString QnMultiserverBookmarksRestHandlerPrivate::urlPath;

QnBookmarkOperation QnMultiserverBookmarksRestHandlerPrivate::getOperation(const QString &path) {
    auto iter = std::find(operations.cbegin(), operations.cend(), path);
    if (iter == operations.cend())
        return QnBookmarkOperation::Get;
    return static_cast<QnBookmarkOperation>(std::distance(operations.cbegin(), iter));
}

QnCameraBookmarkList QnMultiserverBookmarksRestHandlerPrivate::getBookmarks(
    QnCommonModule* commonModule,
    QnGetBookmarksRequestContext& context)
{
    const auto &request = context.request();
    QnMultiServerCameraBookmarkList outputData;
    if (request.isLocal)
    {
        getBookmarksLocal(outputData, &context);
    }
    else
    {
        QSet<QnMediaServerResourcePtr> servers;
        for (const auto& camera: context.request().cameras)
            servers += commonModule->cameraHistoryPool()->getCameraFootageData(camera, true).toSet();

        for (const auto& server: servers)
        {
            if (server->getId() == commonModule->moduleGUID())
                getBookmarksLocal(outputData, &context);
            else
                getBookmarksRemoteAsync(commonModule, outputData, server, &context);
        }
        context.waitForDone();
    }
    const auto result = QnCameraBookmark::mergeCameraBookmarks(
        commonModule,
        outputData,
        request.filter.orderBy,
        request.filter.sparsing,
        request.filter.limit);

    return result;
}


QnCameraBookmarkTagList QnMultiserverBookmarksRestHandlerPrivate::getBookmarkTags(
    QnCommonModule* commonModule,
    QnGetBookmarkTagsRequestContext& context)
{
    const auto &request = context.request();
    QnMultiServerCameraBookmarkTagList outputData;
    if (request.isLocal)
    {
        getBookmarkTagsLocal(outputData, &context);
    }
    else
    {
        for (const auto& server: commonModule->resourcePool()->getAllServers(Qn::Online))
        {
            if (server->getId() == commonModule->moduleGUID())
                getBookmarkTagsLocal(outputData, &context);
            else
                getBookmarkTagsRemoteAsync(commonModule, outputData, server, &context);
        }
        context.waitForDone();
    }
    return QnCameraBookmarkTag::mergeCameraBookmarkTags(outputData, request.limit);
}


bool QnMultiserverBookmarksRestHandlerPrivate::addBookmark(
    QnCommonModule* commonModule,
    QnUpdateBookmarkRequestContext &context,
    const QnUuid& authorityUser)
{
    /* This request always executed locally. */

    auto bookmark = context.request().bookmark;
    bookmark.creatorId = authorityUser;
    bookmark.creationTimeStampMs = qnSyncTime->currentMSecsSinceEpoch();

    if (!qnServerDb->addBookmark(bookmark))
        return false;

    const auto ruleId = context.request().eventRuleId;
    const auto rule = commonModule->eventRuleManager()->rule(ruleId);
    if (!rule)
        return true;

    nx::vms::event::EventParameters runtimeParams;
    runtimeParams.eventResourceId = bookmark.cameraId;
    runtimeParams.eventTimestampUsec = bookmark.startTimeMs * 1000;

    runtimeParams.eventType = rule
        ? rule->eventType()
        : nx::vms::event::EventType::undefinedEvent;

    const auto action = nx::vms::event::CommonAction::create(
        nx::vms::event::ActionType::acknowledgeAction, runtimeParams);

    action->setRuleId(ruleId);

    auto& actionParams = action->getParams();
    actionParams.actionResourceId = authorityUser;
    actionParams.text = bookmark.description;

    qnServerDb->saveActionToDB(action);

    return true;
}

bool QnMultiserverBookmarksRestHandlerPrivate::updateBookmark(
    QnCommonModule* commonModule,
    QnUpdateBookmarkRequestContext &context)
{
    const auto &request = context.request();
    if (request.isLocal)
    {
        qnServerDb->updateBookmark(request.bookmark);
    }
    else
    {
        for (const auto& server: commonModule->resourcePool()->getAllServers(Qn::Online))
        {
            if (server->getId() == commonModule->moduleGUID())
                qnServerDb->updateBookmark(request.bookmark);
            else
                updateBookmarkRemoteAsync(commonModule, server, &context);
        }
        context.waitForDone();
    }
    return true;
}

bool QnMultiserverBookmarksRestHandlerPrivate::deleteBookmark(
    QnCommonModule* commonModule,
    QnDeleteBookmarkRequestContext &context)
{
    const auto &request = context.request();
    if (request.isLocal)
    {
        qnServerDb->deleteBookmark(request.bookmarkId);
    }
    else
    {
        for (const auto& server: commonModule->resourcePool()->getAllServers(Qn::Online))
        {
            if (server->getId() == commonModule->moduleGUID())
                qnServerDb->deleteBookmark(request.bookmarkId);
            else
                deleteBookmarkRemoteAsync(commonModule, server, &context);
        }

        context.waitForDone();
    }
    return true;
}
