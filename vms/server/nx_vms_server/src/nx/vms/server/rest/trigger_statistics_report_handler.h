#pragma once

#include <core/resource/resource_fwd.h>

#include <rest/server/json_rest_handler.h>
#include <rest/helpers/request_context.h>

#include <nx/vms/server/server_module_aware.h>

#include <nx/vms/api/data/statistics_data.h>

namespace nx::vms::server::rest {

class TriggerStatisticsReportHandler: public QnJsonRestHandler, public ServerModuleAware
{
public:
   TriggerStatisticsReportHandler(QnMediaServerModule* serverModule);
   virtual JsonRestResponse executeGet(const JsonRestRequest& request) override;
   virtual JsonRestResponse executePost(const JsonRestRequest& request, const QByteArray& body);

private:
   JsonRestResponse triggerStatisticsReport(
        const nx::vms::api::StatisticsServerArguments& arguments);

   nx::vms::api::StatisticsServerArguments parseBody(const QByteArray& requestBody);
   nx::vms::api::StatisticsServerArguments parseUrlQuery(const QnRequestParams& requestParams);
};


} // namespace nx::vms::server::rest
