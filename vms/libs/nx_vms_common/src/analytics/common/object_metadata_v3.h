// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <analytics/common/object_metadata.h>

namespace nx::common::metadata {

NX_REFLECTION_ENUM_CLASS(ObjectMetadataTypeV3,
    undefined,
    regular,
    bestShot,
    externalBestShot
)

struct ObjectMetadataV3
{
    QString typeId;
    nx::Uuid trackId;
    QRectF boundingBox;
    std::vector<Attribute> attributes;
    ObjectMetadataTypeV3 objectMetadataType;
    nx::Uuid analyticsEngineId;
};
#define ObjectMetadataV3_Fields \
    (typeId) \
    (trackId) \
    (boundingBox) \
    (attributes) \
    (objectMetadataType) \
    (analyticsEngineId)

QN_FUSION_DECLARE_FUNCTIONS(ObjectMetadataV3, (json)(ubjson));

struct ObjectMetadataPacketV3
{
    nx::Uuid deviceId;
    qint64 timestampUs = 0;
    qint64 durationUs = 0;
    std::vector<ObjectMetadataV3> objectMetadataList;
    nx::vms::api::StreamIndex streamIndex = nx::vms::api::StreamIndex::undefined;

    ObjectMetadataPacket toMetadataPacket() const;
};
#define ObjectMetadataPacketV3_Fields \
    (deviceId) \
    (timestampUs) \
    (durationUs) \
    (objectMetadataList) \
    (streamIndex)
QN_FUSION_DECLARE_FUNCTIONS(ObjectMetadataPacketV3, (json)(ubjson));

} // namespace nx::common::metadata
