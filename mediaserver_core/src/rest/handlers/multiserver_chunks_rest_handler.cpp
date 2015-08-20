#include "multiserver_chunks_rest_handler.h"

#include <QUrlQuery>

#include "recording/time_period.h"
#include "recording/time_period_list.h"

#include <api/helpers/chunks_request_data.h>

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
#include "server/server_globals.h"
#include "recorder/storage_manager.h"
#include "http/custom_headers.h"
#include "rest/server/rest_connection_processor.h"
#include "utils/network/tcp_listener.h"

namespace {
    static QString urlPath;
}

struct QnMultiserverChunksRestHandler::InternalContext
{
    InternalContext(const QnChunksRequestData& request, const QnRestConnectionProcessor* owner): request(request), requestsInProgress(0), owner(owner) {}

    const QnChunksRequestData request;
    QnMutex mutex;
    QnWaitCondition waitCond;
    int requestsInProgress;
    const QnRestConnectionProcessor* owner;
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
	//QnRouter::instance()->updateRequest(apiUrl, headers, server->getId());
    headers.emplace(Qn::SERVER_GUID_HEADER_NAME, server->getId().toByteArray());
    QnRoute route = QnRouter::instance()->routeTo(server->getId());
    if (route.reverseConnect) {
        apiUrl.setHost("127.0.0.1"); // proxy via himself to activate backward connection logic
        apiUrl.setPort(ctx->owner->owner()->getPort());
    }
    else if (!route.addr.isNull())
    {
        apiUrl.setHost(route.addr.address.toString());
        apiUrl.setPort(route.addr.port);
    }

    if (QnUserResourcePtr admin = qnResPool->getAdministrator()) {
        apiUrl.setUserName(admin->getName());
        apiUrl.setPassword(QString::fromUtf8(admin->getDigest()));
    }

    QnMutexLocker lock(&ctx->mutex);
    if (nx_http::downloadFileAsync( apiUrl, requestCompletionFunc, headers, nx_http::AsyncHttpClient::authDigestWithPasswordHash ))
        ctx->requestsInProgress++;
}

void QnMultiserverChunksRestHandler::loadLocalData(MultiServerPeriodDataList& outputData, InternalContext* ctx)
{
    MultiServerPeriodData record;
    record.guid = qnCommon->moduleGUID();
    record.periods = qnStorageMan->getRecordedPeriods(ctx->request.resList, ctx->request.startTimeMs, ctx->request.endTimeMs, ctx->request.detailLevel,
        QList<QnServer::ChunksCatalog>() << QnServer::LowQualityCatalog << QnServer::HiQualityCatalog,
        ctx->request.limit);

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

MultiServerPeriodDataList QnMultiserverChunksRestHandler::loadDataSync(const QnChunksRequestData& request, const QnRestConnectionProcessor* owner)
{
    InternalContext ctx(request, owner);
    MultiServerPeriodDataList outputData;
    if (request.isLocal)
    {
        loadLocalData(outputData, &ctx);
    }
    else 
    {
        QSet<QnMediaServerResourcePtr> servers;
        for (const auto& camera: ctx.request.resList)
            servers += qnHistoryPool->getAllCameraServers(camera).toSet();

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

int QnMultiserverChunksRestHandler::executeGet(const QString& path, const QnRequestParamList& params, QByteArray& result, QByteArray& contentType, const QnRestConnectionProcessor* owner)
{
    Q_UNUSED(path);
    MultiServerPeriodDataList outputData;
    QnRestResult restResult;
    QnChunksRequestData request = QnChunksRequestData::fromParams(params);
    if (!request.isValid()) {
        restResult.setError(QnRestResult::InvalidParameter, "Invalid parameters");
        QnFusionRestHandlerDetail::serializeRestReply(outputData, params, result, contentType, restResult);
        return nx_http::StatusCode::ok;
    }
    outputData = loadDataSync(request, owner);

    if (request.flat) {
        std::vector<QnTimePeriodList> periodsList;
        for (const MultiServerPeriodData& value: outputData)
            periodsList.push_back( value.periods );
        QnTimePeriodList timePeriodList = QnTimePeriodList::mergeTimePeriods(periodsList);

        if (request.format == Qn::CompressedPeriodsFormat)
        {
            bool signedSerialization = request.periodsType == Qn::BookmarksContent;
            result = QnCompressedTime::serialized(timePeriodList, signedSerialization);
            contentType = Qn::serializationFormatToHttpContentType(Qn::CompressedPeriodsFormat);
        }
        else {
            QnFusionRestHandlerDetail::serializeRestReply(timePeriodList, params, result, contentType, restResult);
        }

    }
    else {
        if (request.format == Qn::CompressedPeriodsFormat)
        {
            bool signedSerialization = request.periodsType == Qn::BookmarksContent;
            result = QnCompressedTime::serialized(outputData, signedSerialization);
            contentType = Qn::serializationFormatToHttpContentType(Qn::CompressedPeriodsFormat);
        }
        else {
            QnFusionRestHandlerDetail::serializeRestReply(outputData, params, result, contentType, restResult);
        }
    }

    return nx_http::StatusCode::ok;
}
