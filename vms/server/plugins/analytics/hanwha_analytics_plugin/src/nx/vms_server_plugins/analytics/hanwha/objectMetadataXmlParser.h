#pragma once

#include "engine.h"

#include <vector>
#include <string_view>
#include <optional>
#include <algorithm>

#include <QByteArray>

#include <nx/kit/json.h>
#include <nx/sdk/ptr.h>
#include <nx/sdk/analytics/helpers/object_metadata_packet.h>

namespace nx::vms_server_plugins::analytics::hanwha {

//-------------------------------------------------------------------------------------------------

std::vector<nx::sdk::Ptr<nx::sdk::analytics::ObjectMetadataPacket>> parseObjectMetadataXml(
    const QByteArray& data, const Hanwha::EngineManifest& manifest);

//-------------------------------------------------------------------------------------------------

} // namespace nx::vms_server_plugins::analytics::hanwha
