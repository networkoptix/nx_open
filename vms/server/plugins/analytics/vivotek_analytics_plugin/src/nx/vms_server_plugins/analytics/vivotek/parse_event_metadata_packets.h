#pragma once

#include <vector>

#include <nx/sdk/ptr.h>
#include <nx/sdk/analytics/helpers/event_metadata_packet.h>

#include "json_utils.h"

namespace nx::vms_server_plugins::analytics::vivotek {

std::vector<nx::sdk::Ptr<nx::sdk::analytics::EventMetadataPacket>> parseEventMetadataPackets(
    const JsonValue& native);

} // namespace nx::vms_server_plugins::analytics::vivotek
