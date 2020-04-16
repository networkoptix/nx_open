#pragma once

#include <nx/kit/json.h>
#include <nx/sdk/ptr.h>
#include <nx/sdk/analytics/i_object_metadata_packet.h>

namespace nx::vms_server_plugins::analytics::vivotek {

nx::sdk::Ptr<nx::sdk::analytics::IObjectMetadataPacket> parseObjectMetadataPacket(
    nx::kit::Json const& nativeMetadata);

} // namespace nx::vms_server_plugins::analytics::vivotek
