#include "workbench_layout_snapshot_storage.h"
#include <core/resource/layout_resource.h>

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

QnWorkbenchLayoutSnapshotStorage::QnWorkbenchLayoutSnapshotStorage(QObject *parent): 
    QObject(parent) 
{}

QnWorkbenchLayoutSnapshotStorage::~QnWorkbenchLayoutSnapshotStorage() {
    return;
}

const QnWorkbenchLayoutSnapshot &QnWorkbenchLayoutSnapshotStorage::snapshot(const QnLayoutResourcePtr &resource) const {
    QHash<QnLayoutResourcePtr, QnWorkbenchLayoutSnapshot>::const_iterator pos = m_snapshotByLayout.find(resource);
    if(pos == m_snapshotByLayout.end()) {
        qnWarning("No saved snapshot exists for layout '%1'.", resource ? resource->getName() : QLatin1String("null"));
        return m_emptyState;
    }

    return *pos;
}

void QnWorkbenchLayoutSnapshotStorage::store(const QnLayoutResourcePtr &resource) {
    m_snapshotByLayout[resource] = QnWorkbenchLayoutSnapshot(resource);
}

void QnWorkbenchLayoutSnapshotStorage::remove(const QnLayoutResourcePtr &resource) {
    m_snapshotByLayout.remove(resource);
}

void QnWorkbenchLayoutSnapshotStorage::clear() {
    m_snapshotByLayout.clear();
}
