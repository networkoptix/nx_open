#ifndef QN_WORKBENCH_LAYOUT_SNAPSHOT_STORAGE_H
#define QN_WORKBENCH_LAYOUT_SNAPSHOT_STORAGE_H

#include <QObject>
#include <QHash>
#include <core/resource/layout_item_data.h>
#include <core/resource/resource_fwd.h>
#include <utils/common/warnings.h>

class QnWorkbenchLayoutSnapshot {
public:
    QnWorkbenchLayoutSnapshot(): cellAspectRatio(-1.0), cellSpacing(-1.0, -1.0) {}

    QnWorkbenchLayoutSnapshot(const QnLayoutResourcePtr &resource);

    QnLayoutItemDataMap items;
    QString name;
    qreal cellAspectRatio;
    QSizeF cellSpacing;

    friend bool operator==(const QnWorkbenchLayoutSnapshot &l, const QnWorkbenchLayoutSnapshot &r);
};

/**
 * This class provides easy access to snapshots of workbench layouts. 
 */
class QnWorkbenchLayoutSnapshotStorage: public QObject {
    Q_OBJECT;
public:
    QnWorkbenchLayoutSnapshotStorage(QObject *parent = NULL);

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
