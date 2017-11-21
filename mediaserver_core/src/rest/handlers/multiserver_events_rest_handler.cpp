#include "multiserver_events_rest_handler.h"

#include <algorithm>

#include <api/helpers/event_log_request_data.h>
#include <api/helpers/event_log_multiserver_request_data.h>
#include <common/common_module.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource/camera_history.h>
#include <database/server_db.h>
#include <network/tcp_listener.h>
#include <rest/helpers/request_context.h>
#include <rest/helpers/request_helpers.h>
#include <rest/helpers/permissions_helper.h>
#include <rest/server/rest_connection_processor.h>

#include <nx/vms/event/actions/abstract_action.h>
#include <nx/utils/algorithm/merge_sorted_lists.h>

using namespace nx;

//-------------------------------------------------------------------------------------------------
// QnMultiserverEventsRestHandler::Private

class QnMultiserverEventsRestHandler::Private
{
    using MultiData = std::vector<vms::event::ActionDataList>;

public:
    Private(const QString& urlPath): urlPath(urlPath)
    {
    }

    using RequestContext = QnMultiserverRequestContext<QnEventLogMultiserverRequestData>;

    vms::event::ActionDataList getEvents(QnCommonModule* commonModule, RequestContext& context)
    {
        const auto& request = context.request();
        MultiData outputData;

        if (request.isLocal)
        {
            getEventsLocal(outputData, &context);
        }
        else
        {
            const auto servers = commonModule->resourcePool()->getAllServers(Qn::Online);
            for (const auto& server: servers)
            {
                if (server->getId() == commonModule->moduleGUID())
                    getEventsLocal(outputData, &context);
                else
                    getEventsRemoteAsync(commonModule, outputData, server, &context);
            }

            context.waitForDone();
        }

        const auto getTimestamp =
            [](const vms::event::ActionData& data) { return data.eventParams.eventTimestampUsec; };

        return std::move(utils::algorithm::merge_sorted_lists<vms::event::ActionDataList>(
            std::move(outputData), getTimestamp, request.order, request.limit));
    }

private:
    void getEventsLocal(MultiData& outputData, RequestContext* context)
    {
        vms::event::ActionDataList actions = qnServerDb->getActions(context->request().filter);
        if (actions.empty())
            return;

        context->executeGuarded(
            [&actions, &outputData]() { outputData.push_back(std::move(actions)); });
    }

    void getEventsRemoteAsync(
        QnCommonModule* commonModule,
        MultiData& outputData,
        const QnMediaServerResourcePtr& server, RequestContext* context)
    {
        auto requestCompletionFunc =
            [context, &outputData] (SystemError::ErrorCode osErrorCode, int statusCode,
                nx_http::BufferType msgBody )
            {
                vms::event::ActionDataList remoteData;
                bool success = false;

                if (osErrorCode == SystemError::noError && statusCode == nx_http::StatusCode::ok)
                {
                    remoteData = QnUbjson::deserialized(msgBody, remoteData, &success);
                    NX_ASSERT(success, Q_FUNC_INFO, "We should receive correct answer here");
                }

                context->executeGuarded(
                    [context, success, &remoteData, &outputData]()
                    {
                        if (success && !remoteData.empty())
                            outputData.push_back(std::move(remoteData));

                        context->requestProcessed();
                    });
            };

        nx::utils::Url apiUrl(getApiUrl(server));

        QnEventLogMultiserverRequestData modifiedRequest = context->request();
        modifiedRequest.makeLocal();
        apiUrl.setQuery(modifiedRequest.toUrlQuery());

        runMultiserverDownloadRequest(commonModule->router(), apiUrl, server,
            requestCompletionFunc, context);
    }

    nx::utils::Url getApiUrl(const QnMediaServerResourcePtr& server)
    {
        nx::utils::Url apiUrl(server->getApiUrl());
        apiUrl.setPath(L'/' + urlPath);
        return apiUrl;
    }

private:
    const QString urlPath;
};

//-------------------------------------------------------------------------------------------------
// QnMultiserverEventsRestHandler

QnMultiserverEventsRestHandler::QnMultiserverEventsRestHandler(const QString& path):
    d(new Private(path))
{
}

int QnMultiserverEventsRestHandler::executeGet(
    const QString& path,
    const QnRequestParamList& params,
    QByteArray& result,
    QByteArray& contentType,
    const QnRestConnectionProcessor* processor)
{
    const auto commonModule = processor->commonModule();
    const auto ownerPort = processor->owner()->getPort();

    QnEventLogMultiserverRequestData request =
        QnMultiserverRequestData::fromParams<QnEventLogMultiserverRequestData>(
            commonModule->resourcePool(), params);

    QnRestResult restResult;
    nx::vms::event::ActionDataList outputData;

    if (request.isValid())
    {
        Private::RequestContext context(request, ownerPort);
        outputData = d->getEvents(commonModule, context);
    }
    else
    {
        restResult.setError(QnRestResult::InvalidParameter,
            lit("Missing parameter or invalid parameter value"));
    }

    QnFusionRestHandlerDetail::serializeRestReply(outputData,
        params, result, contentType, restResult);

    return nx_http::StatusCode::ok;
}
