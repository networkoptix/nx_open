#include "multiserver_chunks_rest_handler.h"

#include <QUrlQuery>

#include <api/helpers/chunks_request_data.h>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_history.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/user_resource.h>

#include <nx/fusion/model_functions.h>
#include <nx/network/http/asynchttpclient.h>
#include <nx/fusion/serialization/compressed_time.h>

#include <server/server_globals.h>
#include <recorder/storage_manager.h>

#include <network/tcp_listener.h>

#include <rest/server/rest_connection_processor.h>
#include <rest/helpers/chunks_request_helper.h>
#include <rest/helpers/request_context.h>
#include <rest/helpers/request_helpers.h>

namespace
{
    QString urlPath;

    typedef QnMultiserverRequestContext<QnChunksRequestData> QnChunksRequestContext;

    void loadRemoteDataAsync(MultiServerPeriodDataList& outputData, const QnMediaServerResourcePtr &server, QnChunksRequestContext* ctx)
    {

        auto requestCompletionFunc = [ctx, &outputData] (SystemError::ErrorCode osErrorCode, int statusCode, nx_http::BufferType msgBody )
        {
            MultiServerPeriodDataList remoteData;
            bool success = false;
            if( osErrorCode == SystemError::noError && statusCode == nx_http::StatusCode::ok )
                remoteData = QnCompressedTime::deserialized(msgBody, MultiServerPeriodDataList(), &success);

            ctx->executeGuarded([ctx, success, remoteData, &outputData]()
            {
                if (success && !remoteData.empty())
                    outputData.push_back(std::move(remoteData.front()));

                ctx->requestProcessed();
            });
        };

        QUrl apiUrl(server->getApiUrl());
        apiUrl.setPath(L'/' + urlPath + L'/');

        QnChunksRequestData modifiedRequest = ctx->request();
        modifiedRequest.format = Qn::CompressedPeriodsFormat;
        modifiedRequest.isLocal = true;
        apiUrl.setQuery(modifiedRequest.toUrlQuery());

        runMultiserverDownloadRequest(apiUrl, server, requestCompletionFunc, ctx);
    }

    void loadLocalData(MultiServerPeriodDataList& outputData, QnChunksRequestContext* ctx)
    {
        MultiServerPeriodData record;
        record.guid = qnCommon->moduleGUID();
        record.periods = QnChunksRequestHelper::load(ctx->request());

        if (!record.periods.empty()) {
            ctx->executeGuarded([&outputData, record]()
            {
                outputData.push_back(std::move(record));
            });
        }
    }
}

QnMultiserverChunksRestHandler::QnMultiserverChunksRestHandler(const QString& path): QnFusionRestHandler()
{
    urlPath = path;
}


MultiServerPeriodDataList QnMultiserverChunksRestHandler::loadDataSync(const QnChunksRequestData& request, const QnRestConnectionProcessor* owner)
{
    const auto ownerPort = owner->owner()->getPort();
    QnChunksRequestContext ctx(request, ownerPort);
    MultiServerPeriodDataList outputData;
    if (request.isLocal)
    {
        loadLocalData(outputData, &ctx);
    }
    else
    {
        QSet<QnMediaServerResourcePtr> onlineServers;
        for (const auto& camera: ctx.request().resList)
            onlineServers += qnCameraHistoryPool->getCameraFootageData(camera, true).toSet();

        for (const auto& server: onlineServers)
        {
            if (server->getId() == qnCommon->moduleGUID())
                loadLocalData(outputData, &ctx);
            else
                loadRemoteDataAsync(outputData, server, &ctx);
        }

        ctx.waitForDone();
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
            result = QnCompressedTime::serialized(timePeriodList, false);
            contentType = Qn::serializationFormatToHttpContentType(Qn::CompressedPeriodsFormat);
        }
        else {
            QnFusionRestHandlerDetail::serializeRestReply(timePeriodList, params, result, contentType, restResult);
        }

    }
    else {
        if (request.format == Qn::CompressedPeriodsFormat)
        {
            result = QnCompressedTime::serialized(outputData, false);
            contentType = Qn::serializationFormatToHttpContentType(Qn::CompressedPeriodsFormat);
        }
        else {
            QnFusionRestHandlerDetail::serializeRestReply(outputData, params, result, contentType, restResult);
        }
    }

    return nx_http::StatusCode::ok;
}
