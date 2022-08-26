// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "workbench_layout_snapshot_storage.h"

#include <core/resource/layout_resource.h>

bool QnWorkbenchLayoutSnapshotStorage::hasSnapshot(const QnLayoutResourcePtr& layout) const
{
    return m_snapshotByLayout.contains(layout);
}

QnWorkbenchLayoutSnapshot QnWorkbenchLayoutSnapshotStorage::snapshot(
    const QnLayoutResourcePtr& layout) const
{
    if (!NX_ASSERT(layout, "An attempt to get snapshot for null layout"))
        return {};

    NX_ASSERT(hasSnapshot(layout), "No saved snapshot exists for layout '%1'", layout->getName());

    return m_snapshotByLayout.value(layout, QnWorkbenchLayoutSnapshot());
}

void QnWorkbenchLayoutSnapshotStorage::storeSnapshot(const QnLayoutResourcePtr& layout)
{
    if (!NX_ASSERT(layout, "An attempt to store snapshot for null layout"))
        return;

    m_snapshotByLayout.insert(layout, QnWorkbenchLayoutSnapshot(layout));
}

void QnWorkbenchLayoutSnapshotStorage::removeSnapshot(const QnLayoutResourcePtr& layout)
{
    m_snapshotByLayout.remove(layout);
}
