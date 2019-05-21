#include "get_merge_status_handler.h"

#include <network/tcp_connection_priv.h>
#include <utils/common/util.h>
#include <api/global_settings.h>
#include <nx/utils/time.h>
#include <rest/server/rest_connection_processor.h>
#include <common/common_module.h>
#include <nx/network/app_info.h>
#include <nx_ec/ec_api.h>
#include <media_server/media_server_module.h>
#include <network/tcp_listener.h>
#include <nx/utils/elapsed_timer.h>

namespace nx::vms::server::rest {

using namespace std::chrono;

QString GetMergeStatusHandler::kUrlPath("ec2/mergeStatus");

GetMergeStatusHandler::GetMergeStatusHandler(QnMediaServerModule* serverModule):
    nx::vms::server::ServerModuleAware(serverModule)
{
}

void GetMergeStatusHandler::loadRemoteDataAsync(
    std::vector<nx::vms::api::MergeStatusReply>& outputData,
    const QnMediaServerResourcePtr& server,
    QnMultiserverRequestContext<MergeStatusRequestData>* ctx) const
{
    auto requestCompletionFunc =
        [ctx, &outputData, server, this](SystemError::ErrorCode osErrorCode,
            int statusCode, nx::network::http::BufferType msgBody,
            nx::network::http::HttpHeaders /*httpResponseHeaders*/)
    {
        nx::vms::api::MergeStatusReply remoteData;
        bool success = false;
        if (osErrorCode == SystemError::noError && statusCode == nx::network::http::StatusCode::ok)
        {
            QnJsonRestResult jsonReply;
            success = QJson::deserialize(msgBody, &jsonReply)
                && QJson::deserialize(jsonReply.reply, &remoteData);
        }

        ctx->executeGuarded(
            [ctx, success, &remoteData, &outputData]()
        {
            if (success)
                outputData.push_back(std::move(remoteData));

            ctx->requestProcessed();
        });
    };

    nx::utils::Url apiUrl(server->getApiUrl());
    apiUrl.setPath(lit("/%1/").arg(kUrlPath));

    auto modifiedRequest = ctx->request();
    modifiedRequest.isLocal = true;
    apiUrl.setQuery(modifiedRequest.toUrlQuery());

    runMultiserverDownloadRequest(
        serverModule()->commonModule()->router(), apiUrl, server, requestCompletionFunc, ctx);
}

int GetMergeStatusHandler::executeGet(
    const QString& /*path*/,
    const QnRequestParams& params,
    QnJsonRestResult& result,
    const QnRestConnectionProcessor* owner)
{
    MergeStatusRequestData request = QnMultiserverRequestData::fromParams<MergeStatusRequestData>(
        owner->commonModule()->resourcePool(), params);

    nx::vms::api::MergeStatusReply reply;
    auto commonModule = owner->commonModule();
    auto masterId = commonModule->globalSettings()->LastMergeMasterId();
    auto slaveId = commonModule->globalSettings()->LastMergeSlaveId();
    reply.mergeId = masterId.isNull() ? slaveId : masterId;
    if (request.isLocal)
    {
        reply.mergeInProgress = masterId != slaveId;
    }
    else
    {
        std::vector<nx::vms::api::MergeStatusReply> outputData;
        QnMultiserverRequestContext<MergeStatusRequestData> context(
            MergeStatusRequestData(), owner->owner()->getPort());

        auto onlineServers = owner->commonModule()->resourcePool()->getAllServers(Qn::Online);
        for (const auto& server: onlineServers)
            loadRemoteDataAsync(outputData, server, &context);
        context.waitForDone();

        for (const auto& response: outputData)
        {
            if (response.mergeInProgress)
                reply = response;
            else if (response.mergeId != reply.mergeId)
                reply.mergeInProgress = true;
        }
    }

    result.setReply(reply);
    return nx::network::http::StatusCode::ok;
}

} // namespace nx::vms::server::rest
