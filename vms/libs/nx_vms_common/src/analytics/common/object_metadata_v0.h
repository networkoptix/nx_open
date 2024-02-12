// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <analytics/common/object_metadata.h>

namespace nx::common::metadata {

struct ObjectMetadataV0
{
    QString typeId;
    nx::Uuid trackId;
    QRectF boundingBox;
    Attributes attributes;
    bool bestShot = false;
    nx::Uuid analyticsEngineId;
};

#define ObjectMetadataV0_Fields \
    (typeId) \
    (trackId) \
    (boundingBox) \
    (attributes) \
    (bestShot) \
    (analyticsEngineId)

QN_FUSION_DECLARE_FUNCTIONS(ObjectMetadataV0, (json)(ubjson));

struct ObjectMetadataPacketV0
{
    nx::Uuid deviceId;
    qint64 timestampUs = 0;
    qint64 durationUs = 0;
    std::vector<ObjectMetadataV0> objectMetadataList;
    nx::vms::api::StreamIndex streamIndex = nx::vms::api::StreamIndex::undefined;
};

#define ObjectMetadataPacketV0_Fields \
    (deviceId) \
    (timestampUs) \
    (durationUs) \
    (objectMetadataList) \
    (streamIndex)

QN_FUSION_DECLARE_FUNCTIONS(ObjectMetadataPacketV0, (json)(ubjson));

} // namespace nx::common::metadata
