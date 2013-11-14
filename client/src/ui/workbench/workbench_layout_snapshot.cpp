#include "workbench_layout_snapshot.h"

#include <core/resource/layout_resource.h>

QnWorkbenchLayoutSnapshot::QnWorkbenchLayoutSnapshot(const QnLayoutResourcePtr &resource):
    items(resource->getItems()),
    name(resource->getName()),
    cellAspectRatio(resource->cellAspectRatio()),
    cellSpacing(resource->cellSpacing()),
    backgroundSize(resource->backgroundSize()),
    backgroundImageFilename(resource->backgroundImageFilename()),
    backgroundOpacity(resource->backgroundOpacity()),
    locked(resource->locked())
{}

bool operator==(const QnWorkbenchLayoutSnapshot &l, const QnWorkbenchLayoutSnapshot &r) {
    return 
        l.name == r.name &&
        l.items == r.items &&
        qFuzzyCompare(l.cellAspectRatio, r.cellAspectRatio) &&
        qFuzzyCompare(l.cellSpacing, r.cellSpacing) &&
        l.backgroundSize == r.backgroundSize &&
        l.backgroundImageFilename == r.backgroundImageFilename &&
        qFuzzyCompare(l.backgroundOpacity, r.backgroundOpacity) &&
        l.locked == r.locked;
}

