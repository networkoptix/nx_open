#include "multiserver_bookmarks_rest_handler_p.h"

#include <chrono>

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
#include <media_server/media_server_module.h>

using std::chrono::milliseconds;

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
        auto requestCompletionFunc = [ctx] (SystemError::ErrorCode /*osErrorCode*/, int /*statusCode*/,
            nx::network::http::BufferType /*msgBody*/, nx::network::http::HttpHeaders /*httpHeaders*/)
        {
            ctx->executeGuarded([ctx]()
            {
                ctx->requestProcessed();
            });
        };

        runMultiserverDownloadRequest(commonModule->router(), url, server, requestCompletionFunc, ctx);
    }

    void getBookmarksRemoteAsync(
        QnMediaServerModule* serverModule,
        QnMultiServerCameraBookmarkList& outputData,
        const QnMediaServerResourcePtr &server, QnGetBookmarksRequestContext* ctx)
    {
        auto commonModule = serverModule->commonModule();

        auto requestCompletionFunc = [ctx, &outputData](SystemError::ErrorCode osErrorCode,
            int statusCode, nx::network::http::BufferType msgBody, nx::network::http::HttpHeaders /*httpHeaders*/)
        {
            QnCameraBookmarkList remoteData;
            bool success = false;
            if( osErrorCode == SystemError::noError && statusCode == nx::network::http::StatusCode::ok ) {
                remoteData = QnUbjson::deserialized(msgBody, remoteData, &success);
                NX_ASSERT(success, "We should receive correct answer here");
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

    void getBookmarksLocal(
        QnMediaServerModule* serverModule,
        QnMultiServerCameraBookmarkList& outputData, QnGetBookmarksRequestContext* ctx)
    {
        const auto request = ctx->request();
        QnCameraBookmarkList bookmarks;
        if (serverModule->serverDb()->getBookmarks(request.cameras, request.filter, bookmarks) && !bookmarks.empty())
        {
            ctx->executeGuarded([bookmarks, &outputData]()
            {
                outputData.push_back(std::move(bookmarks));
            });
        }
    }

    void getBookmarkTagsRemoteAsync(
        QnMediaServerModule* serverModule,
        QnMultiServerCameraBookmarkTagList& outputData,
        const QnMediaServerResourcePtr &server,
        QnGetBookmarkTagsRequestContext* ctx)
    {
        auto commonModule = serverModule->commonModule();
        auto requestCompletionFunc = [ctx, &outputData](SystemError::ErrorCode osErrorCode,
            int statusCode, nx::network::http::BufferType msgBody, nx::network::http::HttpHeaders /*httpHeaders*/)
        {
            QnCameraBookmarkTagList remoteData;
            bool success = false;
            if( osErrorCode == SystemError::noError && statusCode == nx::network::http::StatusCode::ok ) {
                remoteData = QnUbjson::deserialized(msgBody, remoteData, &success);
                NX_ASSERT(success, "We should receive correct answer here");
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

    void getBookmarkTagsLocal(
        QnMediaServerModule* serverModule,
        QnMultiServerCameraBookmarkTagList& outputData, QnGetBookmarkTagsRequestContext* ctx)
    {
        auto tags = serverModule->serverDb()->getBookmarkTags(ctx->request().limit);
        if (!tags.empty()) {
            ctx->executeGuarded([tags, &outputData]()
            {
                outputData.push_back(std::move(tags));
            });
        }
    }

    void updateBookmarkRemoteAsync(
        QnMediaServerModule* serverModule,
        const QnMediaServerResourcePtr &server,
        QnUpdateBookmarkRequestContext* ctx)
    {
        nx::utils::Url apiUrl(getApiUrl(server, QnBookmarkOperation::Update));

        QnUpdateBookmarkRequestData modifiedRequest = ctx->request();
        modifiedRequest.makeLocal();
        apiUrl.setQuery(modifiedRequest.toUrlQuery());

        sendAsyncRequest(serverModule->commonModule(), server, apiUrl, ctx);
    }

    void deleteBookmarkRemoteAsync(
        QnMediaServerModule* serverModule,
        const QnMediaServerResourcePtr &server,
        QnDeleteBookmarkRequestContext* ctx)
    {
        nx::utils::Url apiUrl(getApiUrl(server, QnBookmarkOperation::Delete));

        QnDeleteBookmarkRequestData modifiedRequest = ctx->request();
        modifiedRequest.makeLocal();
        apiUrl.setQuery(modifiedRequest.toUrlQuery());

        sendAsyncRequest(serverModule->commonModule(), server, apiUrl, ctx);
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
    QnMediaServerModule* serverModule,
    QnGetBookmarksRequestContext& context)
{
    auto commonModule = serverModule->commonModule();
    const auto &request = context.request();
    QnMultiServerCameraBookmarkList outputData;
    if (request.isLocal)
    {
        getBookmarksLocal(serverModule, outputData, &context);
    }
    else
    {
        QSet<QnMediaServerResourcePtr> servers;
        for (const auto& camera: context.request().cameras)
            servers += commonModule->cameraHistoryPool()->getCameraFootageData(camera, true).toSet();

        for (const auto& server: servers)
        {
            if (server->getId() == commonModule->moduleGUID())
                getBookmarksLocal(serverModule, outputData, &context);
            else
                getBookmarksRemoteAsync(serverModule, outputData, server, &context);
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
    QnMediaServerModule* serverModule,
    QnGetBookmarkTagsRequestContext& context)
{
    auto commonModule = serverModule->commonModule();
    const auto &request = context.request();
    QnMultiServerCameraBookmarkTagList outputData;
    if (request.isLocal)
    {
        getBookmarkTagsLocal(serverModule, outputData, &context);
    }
    else
    {
        for (const auto& server: commonModule->resourcePool()->getAllServers(Qn::Online))
        {
            if (server->getId() == commonModule->moduleGUID())
                getBookmarkTagsLocal(serverModule, outputData, &context);
            else
                getBookmarkTagsRemoteAsync(serverModule, outputData, server, &context);
        }
        context.waitForDone();
    }
    return QnCameraBookmarkTag::mergeCameraBookmarkTags(outputData, request.limit);
}

bool QnMultiserverBookmarksRestHandlerPrivate::addBookmark(
    QnMediaServerModule* serverModule,
    QnUpdateBookmarkRequestContext &context,
    const QnUuid& authorityUser)
{
    /* This request always executed locally. */
    auto commonModule = serverModule->commonModule();
    auto bookmark = context.request().bookmark;
    bookmark.creatorId = authorityUser;
    bookmark.creationTimeStampMs = milliseconds(qnSyncTime->currentMSecsSinceEpoch());

    if (!serverModule->serverDb()->addBookmark(bookmark))
        return false;

    const auto ruleId = context.request().eventRuleId;
    const auto rule = commonModule->eventRuleManager()->rule(ruleId);
    if (!rule)
        return true;

    nx::vms::event::EventParameters runtimeParams;
    runtimeParams.eventResourceId = bookmark.cameraId;
    runtimeParams.eventTimestampUsec = bookmark.startTimeMs.count() * 1000;

    runtimeParams.eventType = rule
        ? rule->eventType()
        : nx::vms::api::EventType::undefinedEvent;

    const auto action = nx::vms::event::CommonAction::create(
        nx::vms::api::ActionType::acknowledgeAction, runtimeParams);

    action->setRuleId(ruleId);

    auto& actionParams = action->getParams();
    actionParams.actionResourceId = authorityUser;
    actionParams.text = bookmark.description;

    serverModule->serverDb()->saveActionToDB(action);

    return true;
}

bool QnMultiserverBookmarksRestHandlerPrivate::updateBookmark(
    QnMediaServerModule* serverModule,
    QnUpdateBookmarkRequestContext &context)
{
    auto commonModule = serverModule->commonModule();
    const auto &request = context.request();
    if (request.isLocal)
    {
        serverModule->serverDb()->updateBookmark(request.bookmark);
    }
    else
    {
        for (const auto& server: commonModule->resourcePool()->getAllServers(Qn::Online))
        {
            if (server->getId() == commonModule->moduleGUID())
                serverModule->serverDb()->updateBookmark(request.bookmark);
            else
                updateBookmarkRemoteAsync(serverModule, server, &context);
        }
        context.waitForDone();
    }
    return true;
}

bool QnMultiserverBookmarksRestHandlerPrivate::deleteBookmark(
    QnMediaServerModule* serverModule,
    QnDeleteBookmarkRequestContext &context)
{
    const auto &request = context.request();
    if (request.isLocal)
    {
        serverModule->serverDb()->deleteBookmark(request.bookmarkId);
    }
    else
    {
        for (const auto& server: serverModule->resourcePool()->getAllServers(Qn::Online))
        {
            if (server->getId() == serverModule->commonModule()->moduleGUID())
                serverModule->serverDb()->deleteBookmark(request.bookmarkId);
            else
                deleteBookmarkRemoteAsync(serverModule, server, &context);
        }

        context.waitForDone();
    }
    return true;
}
