#include "multiserver_chunks_rest_handler.h"

#include <api/helpers/chunks_request_data.h>

#include <common/common_module.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/camera_resource.h>
#include <core/resource/camera_history.h>
#include <core/resource/media_server_resource.h>

#include <nx/fusion/model_functions.h>
#include <nx/fusion/serialization/compressed_time_functions.h>
#include <nx/network/deprecated/asynchttpclient.h>
#include <nx/fusion/serialization/compressed_time.h>

#include <recorder/storage_manager.h>

#include <network/tcp_listener.h>

#include <rest/server/rest_connection_processor.h>
#include <rest/helpers/chunks_request_helper.h>
#include <rest/helpers/request_context.h>
#include <rest/helpers/request_helpers.h>

namespace {

static const nx::utils::log::Tag kTag{lit("multiserver_chunks_rest_handler")};

static QString urlPath;

typedef QnMultiserverRequestContext<QnChunksRequestData> QnChunksRequestContext;

static QString toString(const QnSecurityCamResourceList& cameras)
{
    QString result;
    for (const auto& camera : cameras)
    {
        if (!result.isEmpty())
            result += L',';
        result += camera->getUniqueId();
    }
    return result;
}

static void loadRemoteDataAsync(
    QnCommonModule* commonModule,
    MultiServerPeriodDataList& outputData,
    const QnMediaServerResourcePtr& server,
    QnChunksRequestContext* ctx,
    int requestNum, const QElapsedTimer& timer)
{
    auto requestCompletionFunc =
        [ctx, &outputData, server, requestNum, timer](
            SystemError::ErrorCode osErrorCode, int statusCode, nx_http::BufferType msgBody)
        {
            MultiServerPeriodDataList remoteData;
            bool success = false;
            if (osErrorCode == SystemError::noError && statusCode == nx_http::StatusCode::ok)
            {
                remoteData = QnCompressedTime::deserialized(
                    msgBody, MultiServerPeriodDataList(), &success);
            }

            NX_VERBOSE(kTag)
                << "In progress request QnMultiserverChunksRestHandler::loadRemoteDataAsync #"
                << requestNum << ". Got response from server" << server->getId()
                << "osErrorCode=" << osErrorCode << "statusCode=" << statusCode
                << "cameras=" << toString(ctx->request().resList)
                << "timeout=" << timer.elapsed();

            ctx->executeGuarded(
                [ctx, success, &remoteData, &outputData]()
                {
                    if (success && !remoteData.empty())
                        outputData.push_back(std::move(remoteData.front()));

                    ctx->requestProcessed();
                });
        };

    nx::utils::Url apiUrl(server->getApiUrl());
    apiUrl.setPath(lit("/%1/").arg(urlPath));

    QnChunksRequestData modifiedRequest = ctx->request();
    modifiedRequest.format = Qn::CompressedPeriodsFormat;
    modifiedRequest.isLocal = true;
    apiUrl.setQuery(modifiedRequest.toUrlQuery());

    runMultiserverDownloadRequest(commonModule->router(), apiUrl, server, requestCompletionFunc, ctx);
}

static void loadLocalData(
    QnCommonModule* commonModule,
    MultiServerPeriodDataList& outputData,
    QnChunksRequestContext* ctx)
{
    MultiServerPeriodData record;
    record.guid = commonModule->moduleGUID();
    record.periods = QnChunksRequestHelper::load(ctx->request());

    if (!record.periods.empty())
    {
        ctx->executeGuarded(
            [&outputData, &record]()
            {
                outputData.push_back(std::move(record));
            });
    }
}

static std::atomic<int> staticRequestNum;

} // namespace

//-------------------------------------------------------------------------------------------------

QnMultiserverChunksRestHandler::QnMultiserverChunksRestHandler(const QString& path):
    QnFusionRestHandler()
{
    urlPath = path;
}

MultiServerPeriodDataList QnMultiserverChunksRestHandler::loadDataSync(
    const QnChunksRequestData& request, const QnRestConnectionProcessor* owner)
{
    int requestNum = ++staticRequestNum;
    QElapsedTimer timer;
    timer.restart();
    NX_VERBOSE(kTag) << "Start executing request QnMultiserverChunksRestHandler::loadDataSync #"
        << requestNum << ". cameras=" << toString(request.resList);

    auto result = loadDataSync(request, owner, requestNum, timer);
    NX_VERBOSE(kTag) << "Finishing executing request QnMultiserverChunksRestHandler::loadDataSync #"
        << requestNum;

    return result;
}

MultiServerPeriodDataList QnMultiserverChunksRestHandler::loadDataSync(
    const QnChunksRequestData& request,
    const QnRestConnectionProcessor* owner,
    int requestNum,
    QElapsedTimer& timer)
{
    const auto ownerPort = owner->owner()->getPort();
    QnChunksRequestContext ctx(request, ownerPort);
    MultiServerPeriodDataList outputData;
    if (request.isLocal)
    {
        loadLocalData(owner->commonModule(), outputData, &ctx);
        NX_VERBOSE(kTag) << " In progress request QnMultiserverChunksRestHandler::executeGet #"
            << requestNum << ". After loading local data timeout=" << timer.elapsed();
    }
    else
    {
        QSet<QnMediaServerResourcePtr> onlineServers;
        for (const auto& camera: ctx.request().resList)
            onlineServers += owner->commonModule()->cameraHistoryPool()->getCameraFootageData(camera, true).toSet();

        for (const auto& server: onlineServers)
        {
            if (server->getId() == owner->commonModule()->moduleGUID())
            {
                loadLocalData(owner->commonModule(), outputData, &ctx);
                NX_VERBOSE(kTag)
                    << " In progress request QnMultiserverChunksRestHandler::executeGet #"
                    << requestNum << ". After loading local data. timeout=" << timer.elapsed();
            }
            else
            {
                loadRemoteDataAsync(owner->commonModule(), outputData, server, &ctx, requestNum, timer);
                NX_VERBOSE(kTag)
                    << " In progress request QnMultiserverChunksRestHandler::executeGet #"
                    << requestNum << ". After loading remote data from server" << server->getId()
                    << ". timeout=" << timer.elapsed();
            }
        }

        ctx.waitForDone();
    }

    return outputData;
}

int QnMultiserverChunksRestHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParamList& params,
    QByteArray& result,
    QByteArray& contentType,
    const QnRestConnectionProcessor* owner)
{
    MultiServerPeriodDataList outputData;
    QnRestResult restResult;
    QnChunksRequestData request = QnChunksRequestData::fromParams(owner->commonModule()->resourcePool(), params);

    int requestNum = ++staticRequestNum;
    QElapsedTimer timer;
    timer.restart();
    NX_VERBOSE(kTag) << "Start executing request QnMultiserverChunksRestHandler::executeGet #"
        << requestNum << ". cameras=" << toString(request.resList);

    if (!request.isValid())
    {
        restResult.setError(QnRestResult::InvalidParameter, "Invalid parameters");
        QnFusionRestHandlerDetail::serializeRestReply(outputData, params, result, contentType, restResult);
        return nx_http::StatusCode::ok;
    }
    outputData = loadDataSync(request, owner, requestNum, timer);
    NX_VERBOSE(kTag) << " In progress request QnMultiserverChunksRestHandler::executeGet #"
        << requestNum << ". After loading data. timeout=" << timer.elapsed();

    if (request.flat)
    {
        std::vector<QnTimePeriodList> periodsList;
        for (const MultiServerPeriodData& value: outputData)
            periodsList.push_back(value.periods);
        QnTimePeriodList timePeriodList = QnTimePeriodList::mergeTimePeriods(periodsList);

        if (request.format == Qn::CompressedPeriodsFormat)
        {
            result = QnCompressedTime::serialized(timePeriodList, false);
            contentType = Qn::serializationFormatToHttpContentType(Qn::CompressedPeriodsFormat);
        }
        else
        {
            QnFusionRestHandlerDetail::serializeRestReply(
                timePeriodList, params, result, contentType, restResult);
        }

    }
    else
    {
        if (request.format == Qn::CompressedPeriodsFormat)
        {
            result = QnCompressedTime::serialized(outputData, false);
            contentType = Qn::serializationFormatToHttpContentType(Qn::CompressedPeriodsFormat);
        }
        else
        {
            QnFusionRestHandlerDetail::serializeRestReply(
                outputData, params, result, contentType, restResult);
        }
    }

    NX_VERBOSE(kTag) << "Finish executing request QnMultiserverChunksRestHandler::executeGet #"
        << requestNum << ". timeout=" << timer.elapsed();

    return nx_http::StatusCode::ok;
}
