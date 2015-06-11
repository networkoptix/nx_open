#include "multiserver_bookmarks_rest_handler.h"

#include <api/helpers/bookmark_request_data.h>
 
#include <core/resource/camera_bookmark.h>
 
#include <rest/helpers/bookmarks_request_helper.h>
 
#include "common/common_module.h"
 
#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_history.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>

#include "utils/network/router.h"
// #include <utils/common/model_functions.h>
#include <utils/thread/mutex.h>
#include <utils/thread/wait_condition.h>


namespace {
    static QString urlPath;
}

struct QnMultiserverBookmarksRestHandler::InternalContext
{
    InternalContext(const QnBookmarkRequestData& request): request(request), requestsInProgress(0) {}

    const QnBookmarkRequestData request;
    QnMutex mutex;
    QnWaitCondition waitCond;
    int requestsInProgress;
};

QnMultiserverBookmarksRestHandler::QnMultiserverBookmarksRestHandler(const QString& path): QnFusionRestHandler()
{
    urlPath = path;
}

void QnMultiserverBookmarksRestHandler::loadRemoteDataAsync(MultiServerCameraBookmarkList& outputData, const QnMediaServerResourcePtr &server, InternalContext* ctx)
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

    QnBookmarkRequestData modifiedRequest = ctx->request;
    modifiedRequest.isLocal = true;
    apiUrl.setQuery(modifiedRequest.toUrlQuery());

    nx_http::HttpHeaders headers;
	QnRouter::instance()->updateRequest(apiUrl, headers, server->getId());

    QAuthenticator auth;
    if (QnUserResourcePtr admin = qnResPool->getAdministrator()) {
        auth.setUser(admin->getName());
        auth.setPassword(QString::fromUtf8(admin->getDigest()));
    }

    QnMutexLocker lock(&ctx->mutex);
    if (nx_http::downloadFileAsync( apiUrl, requestCompletionFunc, headers, auth))
        ctx->requestsInProgress++;
}

void QnMultiserverBookmarksRestHandler::loadLocalData(MultiServerCameraBookmarkList& outputData, InternalContext* ctx)
{
    auto bookmarks = QnBookmarksRequestHelper::load(ctx->request);
    if (!bookmarks.empty()) {
        QnMutexLocker lock(&ctx->mutex);
        outputData.push_back(std::move(bookmarks));
    }
}

void QnMultiserverBookmarksRestHandler::waitForDone(InternalContext* ctx)
{
    QnMutexLocker lock(&ctx->mutex);
    while (ctx->requestsInProgress > 0)
        ctx->waitCond.wait(&ctx->mutex);
}

QnCameraBookmarkList QnMultiserverBookmarksRestHandler::loadDataSync(const QnBookmarkRequestData& request)
{
    InternalContext ctx(request);
    MultiServerCameraBookmarkList outputData;
    if (request.isLocal)
    {
        loadLocalData(outputData, &ctx);
    }
    else 
    {
        QSet<QnMediaServerResourcePtr> servers;
        for (const auto& camera: ctx.request.cameras)
            servers += qnCameraHistoryPool->getCameraFootageData(camera).toSet();

        for (const auto& server: servers) 
        {
            if (server->getId() == qnCommon->moduleGUID())
                loadLocalData(outputData, &ctx);
            else
                loadRemoteDataAsync(outputData, server, &ctx);
        }
        waitForDone(&ctx);
    }
    return QnCameraBookmark::mergeCameraBookmarks(outputData, request.filter.limit, request.filter.strategy);
}

int QnMultiserverBookmarksRestHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor*)
{
    Q_UNUSED(path);

    QnBookmarkRequestData request = QnBookmarkRequestData::fromParams(params);
    if (!request.isValid())
        return nx_http::StatusCode::badRequest;
    QnCameraBookmarkList outputData = loadDataSync(request);
    //TODO: #GDM #Bookmarks serialization
    //QnFusionRestHandlerDetail::serialize(outputData, params, result, contentType);
    return nx_http::StatusCode::ok;
}
