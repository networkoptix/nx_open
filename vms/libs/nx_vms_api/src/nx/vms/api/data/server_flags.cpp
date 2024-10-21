// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include <iterator>

#include <nx/vms/api/data/server_flags.h>

namespace nx::vms::api {

static constexpr std::pair<ServerFlag, ServerModelFlag> kLut[]{
    {ServerFlag::SF_AdminApiForPowerUsers, ServerModelFlag::allowsAdminApiForPowerUsers},
    {ServerFlag::SF_HasPublicIP, ServerModelFlag::hasInternetConnection},
    {ServerFlag::SF_ArmServer, ServerModelFlag::isArmServer},
    {ServerFlag::SF_Edge, ServerModelFlag::isEdge},
    {ServerFlag::SF_Docker, ServerModelFlag::runsInDocker},
    {ServerFlag::SF_HasBuzzer, ServerModelFlag::supportsBuzzer},
    {ServerFlag::SF_HasFanMonitoringCapability, ServerModelFlag::supportsFanMonitoring},
    {ServerFlag::SF_HasPoeManagementCapability, ServerModelFlag::supportsPoeManagement},
    {ServerFlag::SF_SupportsTranscoding, ServerModelFlag::supportsTranscoding},
};

static_assert(
    nx::reflect::enumeration::allEnumValues<ServerModelFlag>().size() - 1 == std::size(kLut),
    "ServerModelFlags must cover all enums");

ServerModelFlags convertServerFlagsToModel(ServerFlags flags)
{
    ServerModelFlags modelFlags;
    for (auto [oldFlag, newFlag]: kLut)
    {
        if (flags.testFlag(oldFlag))
            modelFlags.setFlag(newFlag, true);
    }
    return modelFlags;
}

ServerFlags convertModelToServerFlags(ServerModelFlags flags)
{
    ServerFlags serverFlags;
    for (auto [oldFlag, newFlag]: kLut)
    {
        if (flags.testFlag(newFlag))
            serverFlags.setFlag(oldFlag, true);
    }
    return serverFlags;
}

} // namespace nx::vms::api
