// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_layout_snapshot_storage.h"

#include <core/resource/layout_resource.h>

QnWorkbenchLayoutSnapshotStorage::QnWorkbenchLayoutSnapshotStorage(QObject *parent):
    QObject(parent)
{}

QnWorkbenchLayoutSnapshotStorage::~QnWorkbenchLayoutSnapshotStorage() {
    return;
}

const QnWorkbenchLayoutSnapshot &QnWorkbenchLayoutSnapshotStorage::snapshot(const QnLayoutResourcePtr &resource) const {
    QHash<QnLayoutResourcePtr, QnWorkbenchLayoutSnapshot>::const_iterator pos = m_snapshotByLayout.find(resource);
    if(pos == m_snapshotByLayout.end()) {
        NX_ASSERT(false, "No saved snapshot exists for layout '%1'.", resource ? resource->getName() : QLatin1String("null"));
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
