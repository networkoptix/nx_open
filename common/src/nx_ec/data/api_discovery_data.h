#pragma once

#include "api_data.h"

#include <nx/vms/api/types/resource_types.h>

#include <network/module_information.h>

namespace nx { namespace vms { namespace discovery { class Manager; } } }
namespace nx { namespace vms { namespace discovery { struct ModuleEndpoint; } } }

namespace ec2 {

    struct ApiDiscoveryData : nx::vms::api::IdData
    {
        ApiDiscoveryData(): ignore(false) {}

        QString url;
        bool ignore;
    };

#define ApiDiscoveryData_Fields IdData_Fields(url)(ignore)(id)

    struct ApiDiscoverPeerData: nx::vms::api::Data
    {
        QString url;
        QnUuid id;
    };
#define ApiDiscoverPeerData_Fields (url)(id)

    struct ApiDiscoveredServerData : QnModuleInformationWithAddresses
    {
        ApiDiscoveredServerData() = default;
        ApiDiscoveredServerData(const QnModuleInformation &other) :
            QnModuleInformationWithAddresses(other)
        {}
        // Should be only Online, Incompatible or Unauthorized
        nx::vms::api::ResourceStatus status = nx::vms::api::ResourceStatus::online;
    };
#define ApiDiscoveredServerData_Fields QnModuleInformationWithAddresses_Fields(status)

std::vector<ApiDiscoveredServerData> getServers(nx::vms::discovery::Manager* manager);
ApiDiscoveredServerData makeServer(
    const nx::vms::discovery::ModuleEndpoint& module, const QnUuid& localSystemId);

} // namespace ec2

Q_DECLARE_METATYPE(ec2::ApiDiscoveryData);
Q_DECLARE_METATYPE(ec2::ApiDiscoveredServerData);
