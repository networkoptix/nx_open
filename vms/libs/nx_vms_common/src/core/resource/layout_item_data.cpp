// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_item_data.h"

#include <nx/utils/math/fuzzy.h>

#include "media_resource.h"

bool QnLayoutItemData::operator==(const QnLayoutItemData& other) const
{
    return (
        uuid == other.uuid
        && resource.id == other.resource.id
        && resource.uniqueId == other.resource.uniqueId
        && flags == other.flags
        && qFuzzyEquals(combinedGeometry, other.combinedGeometry)
        && zoomTargetUuid == other.zoomTargetUuid
        && qFuzzyEquals(zoomRect, other.zoomRect)
        && qFuzzyEquals(rotation, other.rotation)
        && displayInfo == other.displayInfo
        && controlPtz == other.controlPtz
        && displayAnalyticsObjects == other.displayAnalyticsObjects
        && displayRoi == other.displayRoi
        && contrastParams == other.contrastParams
        && dewarpingParams == other.dewarpingParams
        );
}

QnLayoutItemData QnLayoutItemData::createFromResource(const QnResourcePtr& resource)
{
    QnLayoutItemData data;
    data.uuid = QnUuid::createUuid();
    data.resource.id = resource->getId();

    if (resource->hasFlags(Qn::local_media))
        data.resource.uniqueId = resource->getUniqueId();

    if (auto mediaResource = resource.dynamicCast<QnMediaResource>())
        data.rotation = mediaResource->forcedRotation().value_or(0);

    return data;
}
