#include "workbench_layout_snapshot.h"

QnWorkbenchLayoutSnapshot::QnWorkbenchLayoutSnapshot(const QnLayoutResourcePtr &resource):
    items(resource->getItems()),
    name(resource->getName()),
    cellAspectRatio(resource->cellAspectRatio()),
    cellSpacing(resource->cellSpacing())
{}

bool operator==(const QnWorkbenchLayoutSnapshot &l, const QnWorkbenchLayoutSnapshot &r) {
    return 
        l.name == r.name &&
        l.items == r.items &&
        qFuzzyCompare(l.cellAspectRatio, r.cellAspectRatio) &&
        qFuzzyCompare(l.cellSpacing, r.cellSpacing);
}

