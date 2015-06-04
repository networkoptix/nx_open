#include "multiserver_chunks_rest_handler.h"

#include <QUrlQuery>

#include "recording/time_period.h"
#include "recording/time_period_list.h"

#include "chunks/chunks_request_helper.h"
#include "common/common_module.h"

#include "core/resource_management/resource_pool.h"
#include "core/resource/camera_resource.h"
#include "core/resource/camera_history.h"
#include "core/resource/media_server_resource.h"
#include <core/resource/user_resource.h>

#include "utils/network/router.h"
#include <utils/common/model_functions.h>
#include "utils/network/http/asynchttpclient.h"
#include "utils/serialization/compressed_time.h"
#include <utils/thread/mutex.h>
#include <utils/thread/wait_condition.h>


namespace {
    static QString urlPath;
}

struct QnMultiserverChunksRestHandler::InternalContext
{
    InternalContext(const QnChunksRequestData& request): request(request), requestsInProgress(0) {}

    const QnChunksRequestData request;
    QnMutex mutex;
    QnWaitCondition waitCond;
    int requestsInProgress;
};

QnMultiserverChunksRestHandler::QnMultiserverChunksRestHandler(const QString& path): QnFusionRestHandler()
{
    urlPath = path;
}

void QnMultiserverChunksRestHandler::loadRemoteDataAsync(MultiServerPeriodDataList& outputData, const QnMediaServerResourcePtr &server, InternalContext* ctx)
{

    auto requestCompletionFunc = [ctx, &outputData] (SystemError::ErrorCode osErrorCode, int statusCode, nx_http::BufferType msgBody )
    {
        MultiServerPeriodDataList remoteData;
        bool success = false;
        if( osErrorCode == SystemError::noError && statusCode == nx_http::StatusCode::ok )
            remoteData = QnCompressedTime::deserialized(msgBody, MultiServerPeriodDataList(), &success);
        
        QnMutexLocker lock(&ctx->mutex);
        if (success && !remoteData.empty())
            outputData.push_back(std::move(remoteData.front()));
        ctx->requestsInProgress--;
        ctx->waitCond.wakeAll();
    };

    QUrl apiUrl(server->getApiUrl());
    apiUrl.setPath(L'/' + urlPath + L'/');

    QnChunksRequestData modifiedRequest = ctx->request;
    modifiedRequest.format = Qn::CompressedPeriodsFormat;
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

void QnMultiserverChunksRestHandler::loadLocalData(MultiServerPeriodDataList& outputData, InternalContext* ctx)
{
    MultiServerPeriodData record;
    record.guid = qnCommon->moduleGUID();
    record.periods = QnChunksRequestHelper::load(ctx->request);
    if (!record.periods.empty()) {
        QnMutexLocker lock(&ctx->mutex);
        outputData.push_back(std::move(record));
    }
}

void QnMultiserverChunksRestHandler::waitForDone(InternalContext* ctx)
{
    QnMutexLocker lock(&ctx->mutex);
    while (ctx->requestsInProgress > 0)
        ctx->waitCond.wait(&ctx->mutex);
}

MultiServerPeriodDataList QnMultiserverChunksRestHandler::loadDataSync(const QnChunksRequestData& request)
{
    InternalContext ctx(request);
    MultiServerPeriodDataList outputData;
    if (request.isLocal)
    {
        loadLocalData(outputData, &ctx);
    }
    else 
    {
        QSet<QnMediaServerResourcePtr> servers;
        for (const auto& camera: ctx.request.resList)
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
    return outputData;
}

int QnMultiserverChunksRestHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor*)
{
    Q_UNUSED(path);

    QnChunksRequestData request = QnChunksRequestData::fromParams(params);
    if (!request.isValid())
        return nx_http::StatusCode::badRequest;
    MultiServerPeriodDataList outputData = loadDataSync(request);

    if (request.format == Qn::CompressedPeriodsFormat)
    {
        bool signedSerialization = request.periodsType == Qn::BookmarksContent;
        result = QnCompressedTime::serialized(outputData, signedSerialization);
        contentType = Qn::serializationFormatToHttpContentType(Qn::CompressedPeriodsFormat);
    }
    else {
        QnFusionRestHandlerDetail::serialize(outputData, params, result, contentType);
    }

    return nx_http::StatusCode::ok;
}
