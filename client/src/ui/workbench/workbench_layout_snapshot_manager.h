#ifndef QN_WORKBENCH_LAYOUT_SNAPSHOT_MANAGER_H
#define QN_WORKBENCH_LAYOUT_SNAPSHOT_MANAGER_H

#include <QObject>
#include <core/resource/resource_fwd.h>
#include <api/app_server_connection.h>
#include "workbench_context_aware.h"
#include "workbench_globals.h"

class QnWorkbenchContext;
class QnWorkbenchLayoutSnapshotStorage;
class QnWorkbenchLayoutSnapshotManager;

namespace detail {
    class QnWorkbenchLayoutReplyProcessor: public QObject {
        Q_OBJECT

    public:
        QnWorkbenchLayoutReplyProcessor(QnWorkbenchLayoutSnapshotManager *manager, const QnLayoutResourceList &resources): 
            m_manager(manager),
            m_resources(resources)
        {}

    public slots:
        void at_finished(int status, const QByteArray &errorString, const QnResourceList &resources, int handle);

    signals:
        void finished(int status, const QByteArray &errorString, const QnResourceList &resources, int handle);

    private:
        QWeakPointer<QnWorkbenchLayoutSnapshotManager> m_manager;
        QnLayoutResourceList m_resources;
    };

} // namespace detail

/**
 * This class maintains a storage of layout snapshots and tracks the state of
 * each layout.
 * 
 * It also provides some functions for layout and snapshot manipulation.
 */
class QnWorkbenchLayoutSnapshotManager: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT;
public:
    QnWorkbenchLayoutSnapshotManager(QObject *parent = NULL);
    virtual ~QnWorkbenchLayoutSnapshotManager();

    void save(const QnLayoutResourcePtr &resource, QObject *object, const char *slot);
    void save(const QnLayoutResourceList &resources, QObject *object, const char *slot);

    void restore(const QnLayoutResourcePtr &resource);

    Qn::ResourceSavingFlags flags(const QnLayoutResourcePtr &resource) const;
    Qn::ResourceSavingFlags flags(QnWorkbenchLayout *layout) const;

    bool isChanged(const QnLayoutResourcePtr &resource) const {
        return flags(resource) & Qn::ResourceIsChanged;
    }

    bool isLocal(const QnLayoutResourcePtr &resource) const {
        return flags(resource) & Qn::ResourceIsLocal;
    }

    bool isBeingSaved(const QnLayoutResourcePtr &resource) const {
        return flags(resource) & Qn::ResourceIsBeingSaved;
    }

    bool isSaveable(const QnLayoutResourcePtr &resource) const {
        Qn::ResourceSavingFlags flags = this->flags(resource);
        if(flags & Qn::ResourceIsBeingSaved)
            return false;

        if(flags & (Qn::ResourceIsLocal | Qn::ResourceIsChanged))
            return true;

        return false;
    }

    bool isModified(const QnLayoutResourcePtr &resource) const {
        return (flags(resource) & (Qn::ResourceIsChanged | Qn::ResourceIsBeingSaved)) == Qn::ResourceIsChanged; /* Changed and not being saved. */
    }

signals:
    void flagsChanged(const QnLayoutResourcePtr &resource);

protected:
    void setFlags(const QnLayoutResourcePtr &resource, Qn::ResourceSavingFlags flags);

    void connectTo(const QnLayoutResourcePtr &resource);
    void disconnectFrom(const QnLayoutResourcePtr &resource);

    Qn::ResourceSavingFlags defaultFlags(const QnLayoutResourcePtr &resource) const;

    QnAppServerConnectionPtr connection() const;

protected slots:
    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);
    void at_layouts_saved(const QnLayoutResourceList &resources);
    void at_layouts_saveFailed(const QnLayoutResourceList &resources);
    void at_layout_changed(const QnLayoutResourcePtr &resource);
    void at_layout_changed();

private:
    friend class detail::QnWorkbenchLayoutReplyProcessor;

    /** Layout state storage that this object manages. */
    QnWorkbenchLayoutSnapshotStorage *m_storage;

    /** Layout to flags mapping. */
    QHash<QnLayoutResourcePtr, Qn::ResourceSavingFlags> m_flagsByLayout;
};


#endif // QN_WORKBENCH_LAYOUT_SNAPSHOT_MANAGER_H
