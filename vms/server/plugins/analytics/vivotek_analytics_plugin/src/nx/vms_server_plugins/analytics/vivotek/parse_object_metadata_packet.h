#pragma once

#include <nx/sdk/ptr.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>

#include <QtCore/QJsonValue>

namespace nx::vms_server_plugins::analytics::vivotek {

nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadataPacket> parseObjectMetadataPacket(
    const QJsonValue& native);

} // namespace nx::vms_server_plugins::analytics::vivotek
