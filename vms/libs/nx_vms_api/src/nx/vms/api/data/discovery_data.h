#pragma once

#include "id_data.h"
#include "module_information.h"

#include <nx/vms/api/types/resource_types.h>

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API DiscoverPeerData: Data
{
    QString url;
    QnUuid id;
};
#define DiscoverPeerData_Fields \
    (url)(id)

struct NX_VMS_API DiscoveryData: IdData
{
    QString url;
    bool ignore = false;
};
#define DiscoveryData_Fields \
    IdData_Fields (url)(ignore)

struct NX_VMS_API DiscoveredServerData: ModuleInformationWithAddresses
{
    using ModuleInformationWithAddresses::ModuleInformationWithAddresses; //< Forward constructors.

    // Should be only Online, Incompatible or Unauthorized.
    ResourceStatus status = ResourceStatus::online;
};
#define DiscoveredServerData_Fields \
    ModuleInformationWithAddresses_Fields \
    (status)

} // namespace api
} // namespace vms
} // namespace nx

Q_DECLARE_METATYPE(nx::vms::api::DiscoverPeerData)
Q_DECLARE_METATYPE(nx::vms::api::DiscoveryData)
Q_DECLARE_METATYPE(nx::vms::api::DiscoveryDataList)
Q_DECLARE_METATYPE(nx::vms::api::DiscoveredServerData)
Q_DECLARE_METATYPE(nx::vms::api::DiscoveredServerDataList)
