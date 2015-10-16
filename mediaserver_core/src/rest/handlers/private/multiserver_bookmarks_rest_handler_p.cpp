#include "multiserver_bookmarks_rest_handler_p.h"

#include <api/helpers/multiserver_request_data.h>
#include <api/helpers/bookmark_requests.h>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_history.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>

#include <database/server_db.h>

#include <rest/helpers/bookmarks_request_helper.h>

#include <utils/common/model_functions.h>
#include <utils/network/router.h>
#include <utils/thread/mutex.h>
#include <utils/thread/wait_condition.h>

namespace {
    const QString addOperation = lit("add");
    const QString updateOperation = lit("update");
    const QString deleteOperation = lit("delete");
}

struct QnMultiserverBookmarksRestHandlerPrivate::RemoteRequestContext {

    RemoteRequestContext()
        : requestsInProgress(0) 
    {}

    QnMutex mutex;
    QnWaitCondition waitCond;
    int requestsInProgress;
};

struct QnMultiserverBookmarksRestHandlerPrivate::GetBookmarksContext: public RemoteRequestContext {
    GetBookmarksContext(const QnGetBookmarksRequestData& request)
        : RemoteRequestContext()
        , request(request)
    {}

    const QnGetBookmarksRequestData request;

};

struct QnMultiserverBookmarksRestHandlerPrivate::UpdateBookmarkContext: public RemoteRequestContext {
    UpdateBookmarkContext(const QnUpdateBookmarkRequestData &request)
        : RemoteRequestContext()
        , request(request)
    {}
    
    const QnUpdateBookmarkRequestData request;
};

struct QnMultiserverBookmarksRestHandlerPrivate::DeleteBookmarkContext: public RemoteRequestContext {
    DeleteBookmarkContext(const QnDeleteBookmarkRequestData &request)
        : RemoteRequestContext()
        , request(request)
    {}

    const QnDeleteBookmarkRequestData request;
};

QString QnMultiserverBookmarksRestHandlerPrivate::urlPath;

//TODO: #GDM #Bookmarks refactor
QnBookmarkOperation QnMultiserverBookmarksRestHandlerPrivate::getOperation(const QString &path) {
    if (path == addOperation)
        return QnBookmarkOperation::Add;

    if (path == updateOperation)
        return QnBookmarkOperation::Update;

    if (path == deleteOperation)
        return QnBookmarkOperation::Delete;

    return QnBookmarkOperation::Get;
}

void QnMultiserverBookmarksRestHandlerPrivate::getBookmarksRemoteAsync(MultiServerCameraBookmarkList& outputData, const QnMediaServerResourcePtr &server, GetBookmarksContext* ctx)
{
    auto requestCompletionFunc = [ctx, &outputData] (SystemError::ErrorCode osErrorCode, int statusCode, nx_http::BufferType msgBody )
    {
        MultiServerCameraBookmarkList remoteData;
        bool success = false;
        if( osErrorCode == SystemError::noError && statusCode == nx_http::StatusCode::ok )
            remoteData = QJson::deserialized(msgBody, MultiServerCameraBookmarkList(), &success);

        QnMutexLocker lock(&ctx->mutex);
        if (success && !remoteData.empty())
            outputData.push_back(std::move(remoteData.front()));
        ctx->requestsInProgress--;
        ctx->waitCond.wakeAll();
    };

    QUrl apiUrl(server->getApiUrl());
    apiUrl.setPath(L'/' + urlPath + L'/');

    QnGetBookmarksRequestData modifiedRequest = ctx->request;
    modifiedRequest.isLocal = true;
    apiUrl.setQuery(modifiedRequest.toUrlQuery());

    nx_http::HttpHeaders headers;
    QnRouter::instance()->updateRequest(apiUrl, headers, server->getId());

    if (QnUserResourcePtr admin = qnResPool->getAdministrator()) {
        apiUrl.setUserName(admin->getName());
        apiUrl.setPassword(QString::fromUtf8(admin->getDigest()));
    }

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

void QnMultiserverBookmarksRestHandlerPrivate::getBookmarksLocal(MultiServerCameraBookmarkList& outputData, GetBookmarksContext* ctx) {
    auto bookmarks = QnBookmarksRequestHelper::load(ctx->request);
    if (!bookmarks.empty()) {
        QnMutexLocker lock(&ctx->mutex);
        outputData.push_back(std::move(bookmarks));
    }
}

void QnMultiserverBookmarksRestHandlerPrivate::waitForDone(RemoteRequestContext* ctx) {
    QnMutexLocker lock(&ctx->mutex);
    while (ctx->requestsInProgress > 0)
        ctx->waitCond.wait(&ctx->mutex);
}

QnCameraBookmarkList QnMultiserverBookmarksRestHandlerPrivate::getBookmarks(const QnGetBookmarksRequestData& request) {
    GetBookmarksContext ctx(request);
    MultiServerCameraBookmarkList outputData;
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

bool QnMultiserverBookmarksRestHandlerPrivate::addBookmark(const QnUpdateBookmarkRequestData &request) {
    /* This request always executed locally. */
    return qnServerDb->addBookmark(request.bookmark);
}

bool QnMultiserverBookmarksRestHandlerPrivate::updateBookmark(const QnUpdateBookmarkRequestData &request) {
    UpdateBookmarkContext ctx(request);
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

void QnMultiserverBookmarksRestHandlerPrivate::updateBookmarkRemoteAsync(const QnMediaServerResourcePtr &server, UpdateBookmarkContext* ctx) {
    auto requestCompletionFunc = [ctx] (SystemError::ErrorCode osErrorCode, int statusCode, nx_http::BufferType msgBody ) {
        ctx->requestsInProgress--;
        ctx->waitCond.wakeAll();
    };

    QUrl apiUrl(server->getApiUrl());
    apiUrl.setPath(L'/' + urlPath + L'/' + updateOperation);

    QnMultiserverRequestData modifiedRequest = ctx->request;
    modifiedRequest.isLocal = true;
    apiUrl.setQuery(modifiedRequest.toUrlQuery());

    nx_http::HttpHeaders headers;
    QnRouter::instance()->updateRequest(apiUrl, headers, server->getId());

    if (QnUserResourcePtr admin = qnResPool->getAdministrator()) {
        apiUrl.setUserName(admin->getName());
        apiUrl.setPassword(QString::fromUtf8(admin->getDigest()));
    }

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

bool QnMultiserverBookmarksRestHandlerPrivate::deleteBookmark(const QnDeleteBookmarkRequestData &request) {
    DeleteBookmarkContext ctx(request);
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

void QnMultiserverBookmarksRestHandlerPrivate::deleteBookmarkRemoteAsync(const QnMediaServerResourcePtr &server, DeleteBookmarkContext* ctx) {
    auto requestCompletionFunc = [ctx] (SystemError::ErrorCode osErrorCode, int statusCode, nx_http::BufferType msgBody ) {
        ctx->requestsInProgress--;
        ctx->waitCond.wakeAll();
    };

    QUrl apiUrl(server->getApiUrl());
    apiUrl.setPath(L'/' + urlPath + L'/' + deleteOperation);

    QnMultiserverRequestData modifiedRequest = ctx->request;
    modifiedRequest.isLocal = true;
    apiUrl.setQuery(modifiedRequest.toUrlQuery());

    nx_http::HttpHeaders headers;
    QnRouter::instance()->updateRequest(apiUrl, headers, server->getId());

    if (QnUserResourcePtr admin = qnResPool->getAdministrator()) {
        apiUrl.setUserName(admin->getName());
        apiUrl.setPassword(QString::fromUtf8(admin->getDigest()));
    }

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


