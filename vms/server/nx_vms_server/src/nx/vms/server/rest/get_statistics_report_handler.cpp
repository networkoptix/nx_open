#include "get_statistics_report_handler.h"

#include <media_server/media_server_module.h>

#include <nx/vms/server/rest/utils.h>
#include <nx/vms/server/statistics/reporter.h>

namespace nx::vms::server::rest {

GetStatisticsReportHandler::GetStatisticsReportHandler(QnMediaServerModule* serverModule):
    ServerModuleAware(serverModule)
{
}

JsonRestResponse GetStatisticsReportHandler::executeGet(const JsonRestRequest& request)
{
    statistics::Reporter* reporter = serverModule()->statisticsReporter();
    if (!NX_ASSERT(reporter))
    {
        return makeResponse(
            QnRestResult::InternalServerError,
            "Unable to access the statistics reporter");
    }

    nx::vms::api::SystemStatistics statistics;
    if (const ec2::ErrorCode errorCode = reporter->collectReportData(&statistics);
        errorCode != ec2::ErrorCode::ok)
    {
        return makeResponse(
            QnRestResult::InternalServerError,
            lm("Unable to collect data for the report (error code: %1)").args(errorCode));
    }

    return statistics;
}

} // namespace nx::vms::server::rest
