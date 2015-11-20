#include "multiserver_bookmarks_rest_handler_p.h"

#include <api/helpers/multiserver_request_data.h>
#include <api/helpers/bookmark_request_data.h>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_history.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>

#include <database/server_db.h>

#include <rest/helpers/bookmarks_request_helper.h>

#include <network/router.h>

#include <utils/common/model_functions.h>
#include <utils/network/http/asynchttpclient.h>
#include <utils/network/http/asynchttpclient.h>
#include <utils/thread/mutex.h>
#include <utils/thread/wait_condition.h>

namespace {
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

}

QString QnMultiserverBookmarksRestHandlerPrivate::urlPath;

QnBookmarkOperation QnMultiserverBookmarksRestHandlerPrivate::getOperation(const QString &path) {
    auto iter = std::find(operations.cbegin(), operations.cend(), path);
    if (iter == operations.cend())
        return QnBookmarkOperation::Get;
    return static_cast<QnBookmarkOperation>(std::distance(operations.cbegin(), iter));
}

void QnMultiserverBookmarksRestHandlerPrivate::getBookmarksRemoteAsync(QnMultiServerCameraBookmarkList& outputData, const QnMediaServerResourcePtr &server, QnGetBookmarksRequestContext* ctx)
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

    nx_http::HttpHeaders headers;
    QnRouter::instance()->updateRequest(apiUrl, headers, server->getId());

    QnMutexLocker lock(&ctx->mutex);
    if (nx_http::downloadFileAsync(
        apiUrl,
        requestCompletionFunc,
        headers,
        nx_http::AsyncHttpClient::authDigestWithPasswordHash ))
    {
        ctx->requestsInProgress++;
    }
}

void QnMultiserverBookmarksRestHandlerPrivate::getBookmarksLocal(QnMultiServerCameraBookmarkList& outputData, QnGetBookmarksRequestContext* ctx) {
    auto bookmarks = QnBookmarksRequestHelper::loadBookmarks(ctx->request);
    if (!bookmarks.empty()) {
        QnMutexLocker lock(&ctx->mutex);
        outputData.push_back(std::move(bookmarks));
    }
}

void QnMultiserverBookmarksRestHandlerPrivate::getBookmarkTagsRemoteAsync(QnMultiServerCameraBookmarkTagList& outputData, const QnMediaServerResourcePtr &server, QnGetBookmarkTagsRequestContext* ctx) {
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

    nx_http::HttpHeaders headers;
    QnRouter::instance()->updateRequest(apiUrl, headers, server->getId());

    QnMutexLocker lock(&ctx->mutex);
    if (nx_http::downloadFileAsync(
        apiUrl,
        requestCompletionFunc,
        headers,
        nx_http::AsyncHttpClient::authDigestWithPasswordHash ))
    {
        ctx->requestsInProgress++;
    }
}

void QnMultiserverBookmarksRestHandlerPrivate::getBookmarkTagsLocal(QnMultiServerCameraBookmarkTagList& outputData, QnGetBookmarkTagsRequestContext* ctx) {
    auto tags = QnBookmarksRequestHelper::loadTags(ctx->request);
    if (!tags.empty()) {
        QnMutexLocker lock(&ctx->mutex);
        outputData.push_back(std::move(tags));
    }
}


void QnMultiserverBookmarksRestHandlerPrivate::waitForDone(QnMultiserverRequestContext* ctx) {
    QnMutexLocker lock(&ctx->mutex);
    while (ctx->requestsInProgress > 0)
        ctx->waitCond.wait(&ctx->mutex);
}

QnCameraBookmarkList QnMultiserverBookmarksRestHandlerPrivate::getBookmarks(const QnGetBookmarksRequestData& request) {
    QnGetBookmarksRequestContext ctx(request);
    QnMultiServerCameraBookmarkList outputData;
    if (request.isLocal)
    {
        getBookmarksLocal(outputData, &ctx);
    }
    else 
    {
        QSet<QnMediaServerResourcePtr> servers;
        for (const auto& camera: ctx.request.cameras)
            servers += qnCameraHistoryPool->getCameraFootageData(camera).toSet();

        for (const auto& server: servers) 
        {
            if (server->getId() == qnCommon->moduleGUID())
                getBookmarksLocal(outputData, &ctx);
            else
                getBookmarksRemoteAsync(outputData, server, &ctx);
        }
        waitForDone(&ctx);
    }
    return QnCameraBookmark::mergeCameraBookmarks(outputData, request.filter.limit, request.filter.strategy);
}


QnCameraBookmarkTagList QnMultiserverBookmarksRestHandlerPrivate::getBookmarkTags(const QnGetBookmarkTagsRequestData& request) {
    QnGetBookmarkTagsRequestContext ctx(request);
    QnMultiServerCameraBookmarkTagList outputData;
    if (request.isLocal)
    {
        getBookmarkTagsLocal(outputData, &ctx);
    }
    else 
    {
        for (const auto& server: qnResPool->getAllServers()) 
        {
            if (server->getId() == qnCommon->moduleGUID())
                getBookmarkTagsLocal(outputData, &ctx);
            else
                getBookmarkTagsRemoteAsync(outputData, server, &ctx);
        }
        waitForDone(&ctx);
    }
    return QnCameraBookmarkTag::mergeCameraBookmarkTags(outputData, request.limit);
}


bool QnMultiserverBookmarksRestHandlerPrivate::addBookmark(const QnUpdateBookmarkRequestData &request) {
    /* This request always executed locally. */
    return qnServerDb->addBookmark(request.bookmark);
}

bool QnMultiserverBookmarksRestHandlerPrivate::updateBookmark(const QnUpdateBookmarkRequestData &request) {
    QnUpdateBookmarkRequestContext ctx(request);
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
                updateBookmarkRemoteAsync(server, &ctx);
        }
        waitForDone(&ctx);
    }
    return true;
}

void QnMultiserverBookmarksRestHandlerPrivate::updateBookmarkRemoteAsync(const QnMediaServerResourcePtr &server, QnUpdateBookmarkRequestContext* ctx) {
    QUrl apiUrl(getApiUrl(server, QnBookmarkOperation::Update));

    QnUpdateBookmarkRequestData modifiedRequest = ctx->request;
    modifiedRequest.makeLocal();
    apiUrl.setQuery(modifiedRequest.toUrlQuery());

    sendAsyncRequest(server, apiUrl, ctx);
}

bool QnMultiserverBookmarksRestHandlerPrivate::deleteBookmark(const QnDeleteBookmarkRequestData &request) {
    QnDeleteBookmarkRequestContext ctx(request);
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
                deleteBookmarkRemoteAsync(server, &ctx);
        }
        waitForDone(&ctx);
    }
    return true;
}

void QnMultiserverBookmarksRestHandlerPrivate::deleteBookmarkRemoteAsync(const QnMediaServerResourcePtr &server, QnDeleteBookmarkRequestContext* ctx) {
    QUrl apiUrl(getApiUrl(server, QnBookmarkOperation::Delete));

    QnDeleteBookmarkRequestData modifiedRequest = ctx->request;
    modifiedRequest.makeLocal();
    apiUrl.setQuery(modifiedRequest.toUrlQuery());

    sendAsyncRequest(server, apiUrl, ctx);
}

void QnMultiserverBookmarksRestHandlerPrivate::sendAsyncRequest(const QnMediaServerResourcePtr &server, QUrl url, QnMultiserverRequestContext *ctx) {
    auto requestCompletionFunc = [ctx] (SystemError::ErrorCode osErrorCode, int statusCode, nx_http::BufferType msgBody ) {
        QN_UNUSED(osErrorCode, statusCode, msgBody);
        ctx->requestsInProgress--;
        ctx->waitCond.wakeAll();
    };

    nx_http::HttpHeaders headers;
    QnRouter::instance()->updateRequest(url, headers, server->getId());

    QnMutexLocker lock(&ctx->mutex);
    if (nx_http::downloadFileAsync(
        url,
        requestCompletionFunc,
        headers,
        nx_http::AsyncHttpClient::authDigestWithPasswordHash ))
    {
        ctx->requestsInProgress++;
    }
}

