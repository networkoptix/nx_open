#include "api_discovery_data.h"

#include <api/global_settings.h>
#include <common/common_module.h>
#include <network/connection_validator.h>
#include <nx/fusion/model_functions.h>
#include <nx/vms/discovery/manager.h>

namespace ec2 {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ApiDiscoveredServerData), (ubjson)(json), _Fields)

std::vector<ApiDiscoveredServerData> getServers(nx::vms::discovery::Manager* manager)
{
    const auto localSystemId = manager->commonModule()->globalSettings()->localSystemId();

    ApiDiscoveredServerDataList result;
    for (const auto& module: manager->getAll())
        result.push_back(makeServer(module, localSystemId));

    return result;
}

ApiDiscoveredServerData makeServer(
    const nx::vms::discovery::ModuleEndpoint& module, const QnUuid& localSystemId)
{
    ApiDiscoveredServerData serverData(module);
    serverData.setEndpoints(std::vector<nx::network::SocketAddress>{module.endpoint});
    if (QnConnectionValidator::validateConnection(module) != Qn::SuccessConnectionResult)
    {
        serverData.status = nx::vms::api::ResourceStatus::incompatible;
    }
    else
    {
        serverData.status = (!localSystemId.isNull() && module.localSystemId == localSystemId)
            ? nx::vms::api::ResourceStatus::online
            : nx::vms::api::ResourceStatus::unauthorized;
    }

    return serverData;
}
} // namespace ec2
