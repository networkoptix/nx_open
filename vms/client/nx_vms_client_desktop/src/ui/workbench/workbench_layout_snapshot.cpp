#include "workbench_layout_snapshot.h"

#include <core/resource/layout_resource.h>

#include <nx/utils/math/fuzzy.h>

QnWorkbenchLayoutSnapshot::QnWorkbenchLayoutSnapshot()
    :
    cellAspectRatio(-1.0),
    cellSpacing(-1.0),
    backgroundOpacity(),
    locked(false)
{
}

QnWorkbenchLayoutSnapshot::QnWorkbenchLayoutSnapshot(const QnLayoutResourcePtr &resource)
    :
    items(resource->getItems()),
    name(resource->getName()),
    cellAspectRatio(resource->cellAspectRatio()),
    cellSpacing(resource->cellSpacing()),
    backgroundSize(resource->backgroundSize()),
    backgroundImageFilename(resource->backgroundImageFilename()),
    backgroundOpacity(resource->backgroundOpacity()),
    fixedSize(resource->fixedSize()),
    logicalId(resource->logicalId()),
    locked(resource->locked())
{
}

bool operator==(const QnWorkbenchLayoutSnapshot& l, const QnWorkbenchLayoutSnapshot& r)
{
    return l.name == r.name
        && l.items == r.items
        && qFuzzyEquals(l.cellAspectRatio, r.cellAspectRatio)
        && qFuzzyEquals(l.cellSpacing, r.cellSpacing)
        && l.backgroundSize == r.backgroundSize
        && l.backgroundImageFilename == r.backgroundImageFilename
        && qFuzzyEquals(l.backgroundOpacity, r.backgroundOpacity)
        && l.fixedSize == r.fixedSize
        && l.logicalId == r.logicalId
        && l.locked == r.locked;
}

