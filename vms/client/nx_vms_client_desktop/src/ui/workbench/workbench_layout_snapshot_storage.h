// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QHash>

#include <core/resource/resource_fwd.h>

#include "workbench_layout_snapshot.h"

/**
 * This class provides easy access to snapshots of workbench layouts.
 */
class QnWorkbenchLayoutSnapshotStorage
{
public:
    QnWorkbenchLayoutSnapshotStorage() = default;

    bool hasSnapshot(const QnLayoutResourcePtr& layout) const;
    QnWorkbenchLayoutSnapshot snapshot(const QnLayoutResourcePtr& layout) const;
    void storeSnapshot(const QnLayoutResourcePtr& layout);
    void removeSnapshot(const QnLayoutResourcePtr& layout);

private:
    QHash<QnLayoutResourcePtr, QnWorkbenchLayoutSnapshot> m_snapshotByLayout;
};
