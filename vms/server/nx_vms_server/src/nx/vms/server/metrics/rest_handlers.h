#pragma once

#include <core/resource/resource_fwd.h>
#include <nx/vms/server/server_module_aware.h>
#include <nx/vms/utils/metrics/system_controller.h>
#include <rest/server/json_rest_handler.h>

namespace nx::vms::network { class AbstractServerConnector; }
namespace cf { template<typename T> class future; }

namespace nx::vms::server::metrics {

class LocalRestHandler:
    public QnJsonRestHandler
{
public:
    LocalRestHandler(utils::metrics::SystemController* controller);

protected:
    JsonRestResponse executeGet(const JsonRestRequest& request) override;

protected:
    utils::metrics::SystemController* const m_controller;
};

class SystemRestHandler:
    public LocalRestHandler,
    public ServerModuleAware
{
public:
    SystemRestHandler(
        utils::metrics::SystemController* controller,
        QnMediaServerModule* serverModule,
        nx::vms::network::AbstractServerConnector* serverConnector);

protected:
    JsonRestResponse executeGet(const JsonRestRequest& request) override;

private:
    template<typename Values>
    JsonRestResponse getAndMerge(Values values, const QString& api, const QString& query = {});

    cf::future<QJsonValue> getAsync(
        const QnMediaServerResourcePtr& server, const nx::utils::Url& url);

private:
    nx::vms::network::AbstractServerConnector* m_serverConnector = nullptr;
};

} // namespace nx::vms::server
