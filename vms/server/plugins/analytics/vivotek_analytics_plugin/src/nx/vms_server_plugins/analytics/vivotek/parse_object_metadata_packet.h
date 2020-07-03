#pragma once

#include <nx/sdk/ptr.h>
#include <nx/sdk/analytics/i_object_metadata_packet.h>

#include "json_utils.h"

namespace nx::vms_server_plugins::analytics::vivotek {

nx::sdk::Ptr<nx::sdk::analytics::IObjectMetadataPacket> parseObjectMetadataPacket(
    const JsonValue& native);

} // namespace nx::vms_server_plugins::analytics::vivotek
