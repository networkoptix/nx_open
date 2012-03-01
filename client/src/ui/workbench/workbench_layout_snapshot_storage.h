#ifndef QN_WORKBENCH_LAYOUT_SNAPSHOT_STORAGE_H
#define QN_WORKBENCH_LAYOUT_SNAPSHOT_STORAGE_H

#include <QObject>
#include <QHash>
#include <core/resource/layout_item_data.h>
#include <utils/common/warnings.h>

class QnWorkbenchLayoutSnapshot {
public:
    QnWorkbenchLayoutSnapshot() {}

    QnWorkbenchLayoutSnapshot(const QnLayoutResourcePtr &resource):
        items(resource->getItems()),
        name(resource->getName())
    {}

    QnLayoutItemDataMap items;
    QString name;
};


class QnWorkbenchLayoutSnapshotStorage: public QObject {
    Q_OBJECT;
public:
    QnWorkbenchLayoutSnapshotStorage(QObject *parent = NULL): QObject(parent) {}

    const QnWorkbenchLayoutSnapshot &snapshot(const QnLayoutResourcePtr &resource) const {
        QHash<QnLayoutResourcePtr, QnWorkbenchLayoutSnapshot>::const_iterator pos = m_snapshotByLayout.find(resource);
        if(pos == m_snapshotByLayout.end()) {
            qnWarning("No saved snapshot exists for layout '%1'.", resource ? resource->getName() : QLatin1String("null"));
            return m_emptyState;
        }

        return *pos;
    }

    void store(const QnLayoutResourcePtr &resource) {
        m_snapshotByLayout[resource] = QnWorkbenchLayoutSnapshot(resource);
    }

    void remove(const QnLayoutResourcePtr &resource) {
        m_snapshotByLayout.remove(resource);
    }

    void clear() {
        m_snapshotByLayout.clear();
    }

private:
    QHash<QnLayoutResourcePtr, QnWorkbenchLayoutSnapshot> m_snapshotByLayout;
    QnWorkbenchLayoutSnapshot m_emptyState;
};

#endif // QN_WORKBENCH_LAYOUT_SNAPSHOT_STORAGE_H
