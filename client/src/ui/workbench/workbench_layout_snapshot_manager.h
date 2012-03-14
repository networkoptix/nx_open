#ifndef QN_WORKBENCH_LAYOUT_SNAPSHOT_MANAGER_H
#define QN_WORKBENCH_LAYOUT_SNAPSHOT_MANAGER_H

#include <QObject>
#include <core/resource/resource_fwd.h>
#include <api/AppServerConnection.h>
#include "workbench_context_aware.h"

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


namespace Qn {

    enum LayoutFlag {
        /** Layout is local and was never saved to appserver. */
        LayoutIsLocal = 0x1,

        /** Layout is currently being saved to appserver. */
        LayoutIsBeingSaved = 0x2,

        /** LayoutIsLocal unsaved changes are present in the layout. */
        LayoutIsChanged = 0x4,
    };
    Q_DECLARE_FLAGS(LayoutFlags, LayoutFlag);

} // namespace Qn

Q_DECLARE_OPERATORS_FOR_FLAGS(Qn::LayoutFlags);


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

    Qn::LayoutFlags flags(const QnLayoutResourcePtr &resource) const;

    bool isChanged(const QnLayoutResourcePtr &resource) const {
        return flags(resource) & Qn::LayoutIsChanged;
    }

    bool isLocal(const QnLayoutResourcePtr &resource) const {
        return flags(resource) & Qn::LayoutIsLocal;
    }

    bool isBeingSaved(const QnLayoutResourcePtr &resource) const {
        return flags(resource) & Qn::LayoutIsBeingSaved;
    }

    bool isSaveable(const QnLayoutResourcePtr &resource) const {
        Qn::LayoutFlags flags = this->flags(resource);
        if(flags & Qn::LayoutIsBeingSaved)
            return false;

        if(flags & (Qn::LayoutIsLocal | Qn::LayoutIsChanged))
            return true;

        return false;
    }

    bool isModified(const QnLayoutResourcePtr &resource) const {
        return (flags(resource) & (Qn::LayoutIsChanged | Qn::LayoutIsBeingSaved)) == Qn::LayoutIsChanged; /* Changed and not being saved. */
    }

signals:
    void flagsChanged(const QnLayoutResourcePtr &resource);

protected:
    void setFlags(const QnLayoutResourcePtr &resource, Qn::LayoutFlags flags);

    void connectTo(const QnLayoutResourcePtr &resource);
    void disconnectFrom(const QnLayoutResourcePtr &resource);

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
    QHash<QnLayoutResourcePtr, Qn::LayoutFlags> m_flagsByLayout;
};


#endif // QN_WORKBENCH_LAYOUT_SNAPSHOT_MANAGER_H
