// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "object_metadata_v0.h"

#include <nx/fusion/model_functions.h>

namespace nx::common::metadata {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    ObjectMetadataPacketV0, (json)(ubjson), ObjectMetadataPacketV0_Fields, (brief, true))
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    ObjectMetadataV0, (json)(ubjson), ObjectMetadataV0_Fields, (brief, true))

ObjectMetadataPacket ObjectMetadataPacketV0::toMetadataPacket() const
{
    ObjectMetadataPacket newPacket;
    newPacket.deviceId = deviceId;
    newPacket.timestampUs = timestampUs;
    newPacket.durationUs = durationUs;
    for (const auto& oldMetadata: objectMetadataList)
    {
        newPacket.analyticsEngineId = oldMetadata.analyticsEngineId;
        if (oldMetadata.bestShot)
        {
            BestShotMetadata bestShot;
            bestShot.trackId = oldMetadata.trackId;
            bestShot.boundingBox = oldMetadata.boundingBox;
            bestShot.attributes = oldMetadata.attributes;
            newPacket.bestShot = bestShot;
        }
        else
        {
            ObjectMetadata newMetadata;
            newMetadata.typeId = oldMetadata.typeId;
            newMetadata.trackId = oldMetadata.trackId;
            newMetadata.boundingBox = oldMetadata.boundingBox;
            newMetadata.attributes = oldMetadata.attributes;
            newPacket.objectMetadataList.push_back(newMetadata);
        }
    }
    newPacket.streamIndex = streamIndex;
    return newPacket;
}

} // namespace nx::common::metadata
