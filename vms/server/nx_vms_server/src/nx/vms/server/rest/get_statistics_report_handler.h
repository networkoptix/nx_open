#pragma once

#include <core/resource/resource_fwd.h>

#include <rest/server/json_rest_handler.h>
#include <rest/helpers/request_context.h>

#include <nx/vms/server/server_module_aware.h>

namespace nx::vms::server::rest {

class GetStatisticsReportHandler: public QnJsonRestHandler, public ServerModuleAware
{
public:
   GetStatisticsReportHandler(QnMediaServerModule* serverModule);
   virtual JsonRestResponse executeGet(const JsonRestRequest& request) override;
};

} // namespace nx::vms::server::rest
