// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_item_data.h"

#include <nx/utils/math/fuzzy.h>

bool QnLayoutItemData::operator==(const QnLayoutItemData& other) const
{
    return (
        uuid == other.uuid
        && resource.id == other.resource.id
        && resource.path == other.resource.path
        && flags == other.flags
        && qFuzzyEquals(combinedGeometry, other.combinedGeometry)
        && zoomTargetUuid == other.zoomTargetUuid
        && qFuzzyEquals(zoomRect, other.zoomRect)
        && qFuzzyEquals(rotation, other.rotation)
        && displayInfo == other.displayInfo
        && controlPtz == other.controlPtz
        && displayAnalyticsObjects == other.displayAnalyticsObjects
        && displayRoi == other.displayRoi
        && frameDistinctionColor == other.frameDistinctionColor
        && contrastParams == other.contrastParams
        && dewarpingParams == other.dewarpingParams
        );
}
