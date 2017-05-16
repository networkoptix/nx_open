#include "api_discovery_data.h"

#include <nx/fusion/model_functions.h>
#include <nx/vms/discovery/manager.h>

namespace ec2 {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ApiDiscoveryData)(ApiDiscoverPeerData), (ubjson)(xml)(json)(sql_record)(csv_record), _Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (ApiDiscoveredServerData), (ubjson)(xml)(json), _Fields)

std::vector<ApiDiscoveredServerData> getServers(nx::vms::discovery::Manager* manager)
{
    ApiDiscoveredServerDataList result;
    for (const auto& module: manager->getAll())
    {
        ApiDiscoveredServerData serverData(module);
        if (module.endpoint.port == serverData.port)
            serverData.remoteAddresses.insert(module.endpoint.address.toString());
        else
            serverData.remoteAddresses.insert(module.endpoint.toString());

        // TODO: Check if it is compatable?
        serverData.status = Qn::ResourceStatus::Online;
        result.push_back(serverData);
    }

    return result;
}

} // namespace ec2

void serialize_field(const ec2::ApiDiscoveredServerData &, QVariant *) { return; }
void deserialize_field(const QVariant &, ec2::ApiDiscoveredServerData *) { return; }
