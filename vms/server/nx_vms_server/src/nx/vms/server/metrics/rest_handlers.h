#pragma once

#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/utils/metrics/controller.h>
#include <rest/server/json_rest_handler.h>

namespace nx::vms::server::metrics {

class LocalRestHandler:
    public QnJsonRestHandler
{
public:
    LocalRestHandler(utils::metrics::Controller* controller);

protected:
    JsonRestResponse executeGet(const JsonRestRequest& request) override;

protected:
    utils::metrics::Controller* const m_controller;
};

class SystemRestHandler:
    public LocalRestHandler,
    public ServerModuleAware
{
public:
    SystemRestHandler(utils::metrics::Controller* controller, QnMediaServerModule* serverModule);

protected:
    JsonRestResponse executeGet(const JsonRestRequest& request) override;
};

} // namespace nx::vms::server
