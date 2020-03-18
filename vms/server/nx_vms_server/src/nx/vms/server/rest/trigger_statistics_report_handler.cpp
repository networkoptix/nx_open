#include "trigger_statistics_report_handler.h"

#include <media_server/media_server_module.h>

#include <nx/vms/server/rest/utils.h>
#include <nx/vms/server/statistics/reporter.h>

namespace nx::vms::server::rest {

static const QString kRandomSystemIdParameterName = "randomSystemId";

TriggerStatisticsReportHandler::TriggerStatisticsReportHandler(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule)
{
}

JsonRestResponse TriggerStatisticsReportHandler::executeGet(const JsonRestRequest& request)
{
    return triggerStatisticsReport(parseUrlQuery(request.params));
}

JsonRestResponse TriggerStatisticsReportHandler::executePost(
    const JsonRestRequest& /*request*/,
    const QByteArray& body)
{
    return triggerStatisticsReport(parseBody(body));
}

JsonRestResponse TriggerStatisticsReportHandler::triggerStatisticsReport(
    const nx::vms::api::StatisticsServerArguments& arguments)
{
    statistics::Reporter* reporter = serverModule()->statisticsReporter();
    if (!NX_ASSERT(reporter))
    {
        return makeResponse(
            QnRestResult::InternalServerError,
            "Unable to access the statistics reporter");
    }

    nx::vms::api::StatisticsServerInfo outData;
    if (const ec2::ErrorCode errorCode = reporter->triggerStatisticsReport(arguments, &outData);
        errorCode != ec2::ErrorCode::ok)
    {
        return makeResponse(
            QnRestResult::InternalServerError,
            lm("Unable to trigger the report (error code: %1)").args(errorCode));
    }

    return outData;
}

nx::vms::api::StatisticsServerArguments TriggerStatisticsReportHandler::parseBody(
    const QByteArray& requestBody)
{
    nx::vms::api::StatisticsServerArguments result;
    if (QJson::deserialize(requestBody, &result))
        return result;

    return {};
}

nx::vms::api::StatisticsServerArguments TriggerStatisticsReportHandler::parseUrlQuery(
    const QnRequestParams& requestParams)
{
    nx::vms::api::StatisticsServerArguments result;
    if (QnLexical::deserialize(requestParams[kRandomSystemIdParameterName], &result.randomSystemId))
        return result;

    return {};
}

} // namespace nx::vms::server::rest
