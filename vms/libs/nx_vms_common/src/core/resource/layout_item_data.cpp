// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "layout_item_data.h"

#include <nx/utils/math/fuzzy.h>

namespace nx::vms::common {

bool LayoutItemData::operator==(const LayoutItemData& other) const
{
    return resource.id == other.resource.id
        && resource.path == other.resource.path
        // ResourceDescriptor::name comparision skipped intensionally as it is a temporary solution
        // for the cross system layouts which must not affects on LayoutItemData items comparision.
        // TODO: #mmalofeev remove this workaround when cross-system layouts will not
        // require this property any more.
        && uuid == other.uuid
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
        && displayHotspots == other.displayHotspots;
}

} // namespace nx::vms::common
