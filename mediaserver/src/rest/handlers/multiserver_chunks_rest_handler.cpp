#include <QUrlQuery>
#include <QWaitCondition>

#include "multiserver_chunks_rest_handler.h"
#include "recording/time_period.h"
#include "utils/serialization/compressed_time.h"
#include "recording/time_period_list.h"
#include <utils/common/model_functions.h>
#include "chunks/chunks_request_helper.h"
#include "core/resource_management/resource_pool.h"
#include "core/resource/camera_resource.h"
#include "common/common_module.h"
#include "core/resource/camera_history.h"
#include "core/resource/media_server_resource.h"
#include "utils/network/router.h"
#include "utils/network/http/asynchttpclient.h"

struct QnMultiserverChunksRestHandler::InternalContext
{
    InternalContext(): requestsInProgress(0) {}

    QString urlPath;
    QMutex mutex;
    QWaitCondition waitCond;
    int requestsInProgress;
};

void QnMultiserverChunksRestHandler::loadRemoteDataAsync(MultiServerPeriodDataList& outputData, QnMediaServerResourcePtr server, const QnChunksRequestData& request, InternalContext* ctx)
{

    auto requestCompletionFunc = [&] (SystemError::ErrorCode osErrorCode, int statusCode, nx_http::BufferType msgBody )
    {
        if( osErrorCode != SystemError::noError || statusCode != nx_http::StatusCode::ok )
            return;
        bool success = false;
        MultiServerPeriodDataList result = QnCompressedTime::deserialized(msgBody, MultiServerPeriodDataList(), &success);
        QMutexLocker lock(&ctx->mutex);
        if (success && !result.empty())
            outputData.push_back(std::move(result.front()));
        ctx->requestsInProgress--;
        ctx->waitCond.wakeAll();
    };

    QUrl apiUrl(server->getApiUrl());
    apiUrl.setPath(ctx->urlPath);
    QUrlQuery query;
    query.addQueryItem("format", QnLexical::serialized(Qn::CompressedTimePeriods));
    query.addQueryItem("isLocal", QString());
    apiUrl.setQuery(query);

    nx_http::HttpHeaders headers;

    auto route = QnRouter::instance()->routeTo(server->getId());
    if (route.isValid()) {
        apiUrl.setHost(route.points.first().host);
        apiUrl.setPort(route.points.first().port);
        headers.emplace("x-server-guid", server->getId().toByteArray());
    }
    {
        QMutexLocker lock(&ctx->mutex);
        ctx->requestsInProgress++;
    }
    nx_http::downloadFileAsync( route.points.first().url(), requestCompletionFunc, headers);
}

void QnMultiserverChunksRestHandler::loadLocalData(MultiServerPeriodDataList& outputData, const QnChunksRequestData& request, InternalContext* ctx)
{
    MultiServerPeriodData record;
    record.guid = qnCommon->moduleGUID();
    record.periods = QnChunksRequestHelper::load(request);
    if (!record.periods.empty()) {
        QMutexLocker lock(&ctx->mutex);
        outputData.push_back(std::move(record));
    }
}

void QnMultiserverChunksRestHandler::waitForDone(InternalContext* ctx)
{
    QMutexLocker lock(&ctx->mutex);
    while (ctx->requestsInProgress > 0)
        ctx->waitCond.wait(&ctx->mutex);
}

int QnMultiserverChunksRestHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor*)
{
    InternalContext ctx;
    ctx.urlPath = path;

    QnChunksRequestData request = QnChunksRequestData::fromParams(params);

    MultiServerPeriodDataList outputData;

    if (request.isLocal)
    {
        loadLocalData(outputData, request, &ctx);
    }
    else 
    {
        QSet<QnMediaServerResourcePtr> servers;
        for (const auto& camera: request.resList)
            servers += QnCameraHistoryPool::instance()->getServersByCamera(camera).toSet();

        for (const auto& server: servers) 
        {
            if (server->getId() == qnCommon->moduleGUID())
                loadLocalData(outputData, request, &ctx);
            else
                loadRemoteDataAsync(outputData, server, request, &ctx);
        }
        waitForDone(&ctx);
    }

    if (QnFusionRestHandlerDetail::formatFromParams(params) == Qn::CompressedTimePeriods)
    {
        result = QnCompressedTime::serialized(outputData, false);
        contentType = Qn::serializationFormatToHttpContentType(Qn::CompressedTimePeriods);
    }
    else {
        QnFusionRestHandlerDetail::serialize(outputData, params, result, contentType);
    }

    return nx_http::StatusCode::ok;
}
