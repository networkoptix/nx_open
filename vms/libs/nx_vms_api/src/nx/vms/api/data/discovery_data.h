// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/api/types/resource_types.h>

#include "id_data.h"
#include "module_information.h"

namespace nx {
namespace vms {
namespace api {

struct NX_VMS_API DiscoverPeerData
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

    bool operator==(const DiscoveryData& other) const = default;
};
#define DiscoveryData_Fields \
    IdData_Fields (url)(ignore)

struct NX_VMS_API DiscoveredServerData: ModuleInformationWithAddresses
{
    using ModuleInformationWithAddresses::ModuleInformationWithAddresses; //< Forward constructors.

    // Should be only Online, Incompatible or Unauthorized.
    ResourceStatus status = ResourceStatus::online;

    bool operator==(const DiscoveredServerData& other) const = default;
};
#define DiscoveredServerData_Fields \
    ModuleInformationWithAddresses_Fields \
    (status)

NX_VMS_API_DECLARE_STRUCT_EX(DiscoverPeerData, (ubjson)(json)(xml)(csv_record))
NX_VMS_API_DECLARE_STRUCT_AND_LIST_EX(DiscoveryData, (ubjson)(json)(xml)(csv_record)(sql_record))
NX_VMS_API_DECLARE_STRUCT_AND_LIST_EX(DiscoveredServerData, (ubjson)(json)(xml)(csv_record))

} // namespace api
} // namespace vms
} // namespace nx
