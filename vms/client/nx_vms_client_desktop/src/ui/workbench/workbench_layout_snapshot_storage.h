// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#ifndef QN_WORKBENCH_LAYOUT_SNAPSHOT_STORAGE_H
#define QN_WORKBENCH_LAYOUT_SNAPSHOT_STORAGE_H

#include <QtCore/QObject>
#include <QtCore/QHash>

#include <core/resource/resource_fwd.h>

#include "workbench_layout_snapshot.h"


/**
 * This class provides easy access to snapshots of workbench layouts. 
 */
class QnWorkbenchLayoutSnapshotStorage: public QObject {
    Q_OBJECT;
public:
    QnWorkbenchLayoutSnapshotStorage(QObject *parent = nullptr);

    virtual ~QnWorkbenchLayoutSnapshotStorage();

    const QnWorkbenchLayoutSnapshot &snapshot(const QnLayoutResourcePtr &resource) const;

    void store(const QnLayoutResourcePtr &resource);

    void remove(const QnLayoutResourcePtr &resource);

    void clear();

private:
    QHash<QnLayoutResourcePtr, QnWorkbenchLayoutSnapshot> m_snapshotByLayout;
    QnWorkbenchLayoutSnapshot m_emptyState;
};

#endif // QN_WORKBENCH_LAYOUT_SNAPSHOT_STORAGE_H
