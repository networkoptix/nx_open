#include "layout_item_data.h"

#include <nx/utils/math/fuzzy.h>

bool operator==(const QnLayoutItemData &l, const QnLayoutItemData &r)
{
    return (
        l.uuid == r.uuid
        && l.resource.id == r.resource.id
        && l.resource.uniqueId == r.resource.uniqueId
        && l.flags == r.flags
        && qFuzzyEquals(l.combinedGeometry, r.combinedGeometry)
        && l.zoomTargetUuid == r.zoomTargetUuid
        && qFuzzyEquals(l.zoomRect, r.zoomRect)
        && qFuzzyEquals(l.rotation, r.rotation)
        && l.displayInfo == r.displayInfo
        && l.contrastParams == r.contrastParams
        && l.dewarpingParams == r.dewarpingParams
        );
}
