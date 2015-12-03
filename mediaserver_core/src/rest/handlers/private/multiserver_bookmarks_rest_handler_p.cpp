#include "multiserver_bookmarks_rest_handler_p.h"

#include <api/helpers/multiserver_request_data.h>
#include <api/helpers/bookmark_request_data.h>
#include <http/custom_headers.h>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_history.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>

#include <database/server_db.h>

#include <rest/server/rest_connection_processor.h>

#include <network/router.h>
#include <network/tcp_listener.h>

#include <utils/common/model_functions.h>
#include <utils/network/http/asynchttpclient.h>
#include <utils/network/http/asynchttpclient.h>
#include <utils/thread/mutex.h>
#include <utils/thread/wait_condition.h>

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

        if (QnUserResourcePtr admin = qnResPool->getAdministrator()) {
            apiUrl.setUserName(admin->getName());
            apiUrl.setPassword(QString::fromUtf8(admin->getDigest()));
        }

        return apiUrl;
    }

    typedef std::function<void (SystemError::ErrorCode osErrorCode, int statusCode, nx_http::BufferType msgBody)> CompletionFunc;

    template<typename Context>
    void someFunc(QUrl &url
        , const QnMediaServerResourcePtr &server
        , const CompletionFunc &requestCompletionFunc
        , Context* ctx)
    {
        nx_http::HttpHeaders headers;
        headers.emplace(Qn::SERVER_GUID_HEADER_NAME, server->getId().toByteArray());
        const QnRoute route = QnRouter::instance()->routeTo(server->getId());
        if (route.reverseConnect)
        {
            url.setHost("127.0.0.1"); // proxy via himself to activate backward connection logic
            url.setPort(ctx->owner->owner()->getPort());
        }
        else if (!route.addr.isNull())
        {
            url.setHost(route.addr.address.toString());
            url.setPort(route.addr.port);
        }

        QnMutexLocker lock(&ctx->mutex);
        if (nx_http::downloadFileAsync(url, requestCompletionFunc, headers,
            nx_http::AsyncHttpClient::authDigestWithPasswordHash))
        {
            ctx->requestsInProgress++;
        }
    }


    template<typename Context>
    void sendAsyncRequest(const QnMediaServerResourcePtr &server, QUrl url, Context *ctx) {
        auto requestCompletionFunc = [ctx] (SystemError::ErrorCode osErrorCode, int statusCode, nx_http::BufferType msgBody ) {
            QN_UNUSED(osErrorCode, statusCode, msgBody);
            ctx->requestsInProgress--;
            ctx->waitCond.wakeAll();
        };

        someFunc(url, server, requestCompletionFunc, ctx);
    }

    void getBookmarksRemoteAsync(QnMultiServerCameraBookmarkList& outputData, const QnMediaServerResourcePtr &server, QnGetBookmarksRequestContext* ctx)
    {
        auto requestCompletionFunc = [ctx, &outputData] (SystemError::ErrorCode osErrorCode, int statusCode, nx_http::BufferType msgBody )
        {
            QnCameraBookmarkList remoteData;
            bool success = false;
            if( osErrorCode == SystemError::noError && statusCode == nx_http::StatusCode::ok ) {
                remoteData = QnUbjson::deserialized(msgBody, remoteData, &success);
                Q_ASSERT_X(success, Q_FUNC_INFO, "We should receive correct answer here");
            }

            QnMutexLocker lock(&ctx->mutex);
            if (success && !remoteData.empty())
                outputData.push_back(std::move(remoteData));
            ctx->requestsInProgress--;
            ctx->waitCond.wakeAll();
        };

        QUrl apiUrl(getApiUrl(server, QnBookmarkOperation::Get));

        QnGetBookmarksRequestData modifiedRequest = ctx->request;
        modifiedRequest.makeLocal();
        apiUrl.setQuery(modifiedRequest.toUrlQuery());

        someFunc(apiUrl, server, requestCompletionFunc, ctx);
    }

    void getBookmarksLocal(QnMultiServerCameraBookmarkList& outputData, QnGetBookmarksRequestContext* ctx) {
        auto bookmarks = QnBookmarksRequestHelper::loadBookmarks(ctx->request);
        if (!bookmarks.empty()) {
            QnMutexLocker lock(&ctx->mutex);
            outputData.push_back(std::move(bookmarks));
        }
    }

    void getBookmarkTagsRemoteAsync(QnMultiServerCameraBookmarkTagList& outputData, const QnMediaServerResourcePtr &server, QnGetBookmarkTagsRequestContext* ctx) {
        auto requestCompletionFunc = [ctx, &outputData] (SystemError::ErrorCode osErrorCode, int statusCode, nx_http::BufferType msgBody )
        {
            QnCameraBookmarkTagList remoteData;
            bool success = false;
            if( osErrorCode == SystemError::noError && statusCode == nx_http::StatusCode::ok ) {
                remoteData = QnUbjson::deserialized(msgBody, remoteData, &success);
                Q_ASSERT_X(success, Q_FUNC_INFO, "We should receive correct answer here");
            }

            QnMutexLocker lock(&ctx->mutex);
            if (success && !remoteData.empty())
                outputData.push_back(std::move(remoteData));
            ctx->requestsInProgress--;
            ctx->waitCond.wakeAll();
        };

        QUrl apiUrl(getApiUrl(server, QnBookmarkOperation::GetTags));

        QnGetBookmarkTagsRequestData modifiedRequest = ctx->request;
        modifiedRequest.makeLocal();
        apiUrl.setQuery(modifiedRequest.toUrlQuery());

        someFunc(apiUrl, server, requestCompletionFunc, ctx);
    }

    void getBookmarkTagsLocal(QnMultiServerCameraBookmarkTagList& outputData, QnGetBookmarkTagsRequestContext* ctx) {
        auto tags = QnBookmarksRequestHelper::loadTags(ctx->request);
        if (!tags.empty()) {
            QnMutexLocker lock(&ctx->mutex);
            outputData.push_back(std::move(tags));
        }
    }

    void updateBookmarkRemoteAsync(const QnMediaServerResourcePtr &server, QnUpdateBookmarkRequestContext* ctx) {
        QUrl apiUrl(getApiUrl(server, QnBookmarkOperation::Update));

        QnUpdateBookmarkRequestData modifiedRequest = ctx->request;
        modifiedRequest.makeLocal();
        apiUrl.setQuery(modifiedRequest.toUrlQuery());

        sendAsyncRequest(server, apiUrl, ctx);
    }

    void deleteBookmarkRemoteAsync(const QnMediaServerResourcePtr &server, QnDeleteBookmarkRequestContext* ctx) {
        QUrl apiUrl(getApiUrl(server, QnBookmarkOperation::Delete));

        QnDeleteBookmarkRequestData modifiedRequest = ctx->request;
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
    auto &request = context.request;
    QnMultiServerCameraBookmarkList outputData;
    if (request.isLocal)
    {
        getBookmarksLocal(outputData, &context);
    }
    else
    {
        QSet<QnMediaServerResourcePtr> servers;
        for (const auto& camera: context.request.cameras)
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
    return QnCameraBookmark::mergeCameraBookmarks(outputData, request.filter.limit, request.filter.strategy);
}


QnCameraBookmarkTagList QnMultiserverBookmarksRestHandlerPrivate::getBookmarkTags(QnGetBookmarkTagsRequestContext& context) {
    auto &request = context.request;
    QnMultiServerCameraBookmarkTagList outputData;
    if (request.isLocal)
    {
        getBookmarkTagsLocal(outputData, &context);
    }
    else
    {
        for (const auto& server: qnResPool->getAllServers())
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
    return qnServerDb->addBookmark(context.request.bookmark);
}

bool QnMultiserverBookmarksRestHandlerPrivate::updateBookmark(QnUpdateBookmarkRequestContext &context) {
    auto &request = context.request;
    if (request.isLocal)
    {
        qnServerDb->updateBookmark(request.bookmark);
    }
    else
    {
        for (const auto& server: qnResPool->getAllServers())
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
    auto &request = context.request;
    if (request.isLocal)
    {
        qnServerDb->deleteBookmark(request.bookmarkId);
    }
    else
    {
        for (const auto& server: qnResPool->getAllServers())
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