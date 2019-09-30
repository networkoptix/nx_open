#pragma once

#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/utils/metrics/system_controller.h>
#include <nx/network/rest/handler.h>

namespace nx::vms::server::metrics {

class LocalRestHandler:
    public nx::network::rest::Handler
{
public:
    LocalRestHandler(utils::metrics::SystemController* controller);

protected:
    nx::network::rest::Response executeGet(const nx::network::rest::Request& request) override;

protected:
    utils::metrics::SystemController* const m_controller;
};

class SystemRestHandler:
    public LocalRestHandler,
    public ServerModuleAware
{
public:
    SystemRestHandler(utils::metrics::SystemController* controller, QnMediaServerModule* serverModule);

protected:
    nx::network::rest::Response executeGet(const nx::network::rest::Request& request) override;
};

} // namespace nx::vms::server
