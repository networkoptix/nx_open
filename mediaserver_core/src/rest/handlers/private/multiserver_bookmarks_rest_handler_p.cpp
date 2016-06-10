#include "multiserver_bookmarks_rest_handler_p.h"

#include <api/helpers/multiserver_request_data.h>
#include <api/helpers/bookmark_request_data.h>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_history.h>
#include <core/resource/media_server_resource.h>

#include <database/server_db.h>

#include <rest/helpers/request_helpers.h>
#include <rest/server/rest_connection_processor.h>

#include <nx/fusion/model_functions.h>
#include <nx/network/http/asynchttpclient.h>
#include <nx/network/http/asynchttpclient.h>
#include <nx/utils/thread/mutex.h>
#include <nx/utils/thread/wait_condition.h>

namespace
{
    static std::array<QString, static_cast<int>(QnBookmarkOperation::Count)>  operations =
    {
        QString(),
        "tags",
        "add",
        "update",
        "delete",
    };

    QUrl getApiUrl(const QnMediaServerResourcePtr &server, QnBookmarkOperation operation) {
        QUrl apiUrl(server->getApiUrl());
        apiUrl.setPath(L'/' + QnMultiserverBookmarksRestHandlerPrivate::urlPath + L'/' + operations[static_cast<int>(operation)]);
        return apiUrl;
    }

    template<typename Context>
    void sendAsyncRequest(const QnMediaServerResourcePtr &server, QUrl url, Context *ctx) {
        auto requestCompletionFunc = [ctx] (SystemError::ErrorCode osErrorCode, int statusCode, nx_http::BufferType msgBody ) {
            QN_UNUSED(osErrorCode, statusCode, msgBody);
            ctx->executeGuarded([ctx]()
            {
                ctx->requestProcessed();
            });
        };

        runMultiserverDownloadRequest(url, server, requestCompletionFunc, ctx);
    }

    void getBookmarksRemoteAsync(QnMultiServerCameraBookmarkList& outputData, const QnMediaServerResourcePtr &server, QnGetBookmarksRequestContext* ctx)
    {
        auto requestCompletionFunc = [ctx, &outputData] (SystemError::ErrorCode osErrorCode, int statusCode, nx_http::BufferType msgBody )
        {
            QnCameraBookmarkList remoteData;
            bool success = false;
            if( osErrorCode == SystemError::noError && statusCode == nx_http::StatusCode::ok ) {
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

        QUrl apiUrl(getApiUrl(server, QnBookmarkOperation::Get));

        QnGetBookmarksRequestData modifiedRequest = ctx->request();
        modifiedRequest.makeLocal();
        apiUrl.setQuery(modifiedRequest.toUrlQuery());

        runMultiserverDownloadRequest(apiUrl, server, requestCompletionFunc, ctx);
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

    void getBookmarkTagsRemoteAsync(QnMultiServerCameraBookmarkTagList& outputData, const QnMediaServerResourcePtr &server, QnGetBookmarkTagsRequestContext* ctx) {
        auto requestCompletionFunc = [ctx, &outputData] (SystemError::ErrorCode osErrorCode, int statusCode, nx_http::BufferType msgBody )
        {
            QnCameraBookmarkTagList remoteData;
            bool success = false;
            if( osErrorCode == SystemError::noError && statusCode == nx_http::StatusCode::ok ) {
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

        QUrl apiUrl(getApiUrl(server, QnBookmarkOperation::GetTags));

        QnGetBookmarkTagsRequestData modifiedRequest = ctx->request();
        modifiedRequest.makeLocal();
        apiUrl.setQuery(modifiedRequest.toUrlQuery());

        runMultiserverDownloadRequest(apiUrl, server, requestCompletionFunc, ctx);
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

    void updateBookmarkRemoteAsync(const QnMediaServerResourcePtr &server, QnUpdateBookmarkRequestContext* ctx) {
        QUrl apiUrl(getApiUrl(server, QnBookmarkOperation::Update));

        QnUpdateBookmarkRequestData modifiedRequest = ctx->request();
        modifiedRequest.makeLocal();
        apiUrl.setQuery(modifiedRequest.toUrlQuery());

        sendAsyncRequest(server, apiUrl, ctx);
    }

    void deleteBookmarkRemoteAsync(const QnMediaServerResourcePtr &server, QnDeleteBookmarkRequestContext* ctx) {
        QUrl apiUrl(getApiUrl(server, QnBookmarkOperation::Delete));

        QnDeleteBookmarkRequestData modifiedRequest = ctx->request();
        modifiedRequest.makeLocal();
        apiUrl.setQuery(modifiedRequest.toUrlQuery());

        sendAsyncRequest(server, apiUrl, ctx);
    }
}

QString QnMultiserverBookmarksRestHandlerPrivate::urlPath;

QnBookmarkOperation QnMultiserverBookmarksRestHandlerPrivate::getOperation(const QString &path) {
    auto iter = std::find(operations.cbegin(), operations.cend(), path);
    if (iter == operations.cend())
        return QnBookmarkOperation::Get;
    return static_cast<QnBookmarkOperation>(std::distance(operations.cbegin(), iter));
}

QnCameraBookmarkList QnMultiserverBookmarksRestHandlerPrivate::getBookmarks(QnGetBookmarksRequestContext& context) {
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
            servers += qnCameraHistoryPool->getCameraFootageData(camera, true).toSet();

        for (const auto& server: servers)
        {
            if (server->getId() == qnCommon->moduleGUID())
                getBookmarksLocal(outputData, &context);
            else
                getBookmarksRemoteAsync(outputData, server, &context);
        }
        context.waitForDone();
    }
    const auto result = QnCameraBookmark::mergeCameraBookmarks(outputData, request.filter.orderBy
        , request.filter.sparsing, request.filter.limit);

    return result;
}


QnCameraBookmarkTagList QnMultiserverBookmarksRestHandlerPrivate::getBookmarkTags(QnGetBookmarkTagsRequestContext& context) {
    const auto &request = context.request();
    QnMultiServerCameraBookmarkTagList outputData;
    if (request.isLocal)
    {
        getBookmarkTagsLocal(outputData, &context);
    }
    else
    {
        for (const auto& server: qnResPool->getAllServers(Qn::Online))
        {
            if (server->getId() == qnCommon->moduleGUID())
                getBookmarkTagsLocal(outputData, &context);
            else
                getBookmarkTagsRemoteAsync(outputData, server, &context);
        }
        context.waitForDone();
    }
    return QnCameraBookmarkTag::mergeCameraBookmarkTags(outputData, request.limit);
}


bool QnMultiserverBookmarksRestHandlerPrivate::addBookmark(QnUpdateBookmarkRequestContext &context) {
    /* This request always executed locally. */
    return qnServerDb->addBookmark(context.request().bookmark);
}

bool QnMultiserverBookmarksRestHandlerPrivate::updateBookmark(QnUpdateBookmarkRequestContext &context) {
    const auto &request = context.request();
    if (request.isLocal)
    {
        qnServerDb->updateBookmark(request.bookmark);
    }
    else
    {
        for (const auto& server: qnResPool->getAllServers(Qn::Online))
        {
            if (server->getId() == qnCommon->moduleGUID())
                qnServerDb->updateBookmark(request.bookmark);
            else
                updateBookmarkRemoteAsync(server, &context);
        }
        context.waitForDone();
    }
    return true;
}

bool QnMultiserverBookmarksRestHandlerPrivate::deleteBookmark(QnDeleteBookmarkRequestContext &context) {
    const auto &request = context.request();
    if (request.isLocal)
    {
        qnServerDb->deleteBookmark(request.bookmarkId);
    }
    else
    {
        for (const auto& server: qnResPool->getAllServers(Qn::Online))
        {
            if (server->getId() == qnCommon->moduleGUID())
                qnServerDb->deleteBookmark(request.bookmarkId);
            else
                deleteBookmarkRemoteAsync(server, &context);
        }

        context.waitForDone();
    }
    return true;
}