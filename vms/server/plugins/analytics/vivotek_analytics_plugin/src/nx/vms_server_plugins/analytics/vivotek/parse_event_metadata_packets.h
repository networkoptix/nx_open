#pragma once

#include <vector>

#include <nx/sdk/ptr.h>
#include <nx/sdk/analytics/i_event_metadata_packet.h>

#include <QtCore/QJsonValue>

namespace nx::vms_server_plugins::analytics::vivotek {

std::vector<nx::sdk::Ptr<nx::sdk::analytics::IEventMetadataPacket>> parseEventMetadataPackets(
    const QJsonValue& native);

} // namespace nx::vms_server_plugins::analytics::vivotek
