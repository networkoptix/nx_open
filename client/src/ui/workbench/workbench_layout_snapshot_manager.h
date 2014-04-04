#ifndef QN_WORKBENCH_LAYOUT_SNAPSHOT_MANAGER_H
#define QN_WORKBENCH_LAYOUT_SNAPSHOT_MANAGER_H

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <core/resource/layout_resource.h>
#include <api/app_server_connection.h>

#include <client/client_globals.h>

#include "workbench_context_aware.h"

class QnWorkbenchContext;
class QnWorkbenchLayoutSnapshotStorage;
class QnWorkbenchLayoutSnapshotManager;

class QnWorkbenchLayoutReplyProcessor: public QnAbstractReplyProcessor {
    Q_OBJECT

public:
    QnWorkbenchLayoutReplyProcessor(QnWorkbenchLayoutSnapshotManager *manager, const QnLayoutResourceList &layouts):
        QnAbstractReplyProcessor(0),
        m_manager(manager),
        m_layouts(layouts)
    {}

public slots:
    void processReply( int reqID, ec2::ErrorCode errorCode );

signals:
    void finished(int status, const QnResourceList &resources, int handle);

private:
    friend class QnAbstractReplyProcessor;

    QPointer<QnWorkbenchLayoutSnapshotManager> m_manager;
    QnLayoutResourceList m_layouts;
};


/**
 * This class maintains a storage of layout snapshots and tracks the state of
 * each layout.
 * 
 * It also provides some functions for layout and snapshot manipulation.
 */
class QnWorkbenchLayoutSnapshotManager: public Connective<QObject>, public QnWorkbenchContextAware {
    Q_OBJECT
    typedef Connective<QObject> base_type;

public:
    QnWorkbenchLayoutSnapshotManager(QObject *parent = NULL);
    virtual ~QnWorkbenchLayoutSnapshotManager();

    int save(const QnLayoutResourcePtr &resource, QObject *object = NULL, const char *slot = NULL);
    int save(const QnLayoutResourceList &resources, QObject *object = NULL, const char *slot = NULL);

    void store(const QnLayoutResourcePtr &resource);
    void restore(const QnLayoutResourcePtr &resource);

    Qn::ResourceSavingFlags flags(const QnLayoutResourcePtr &resource) const;
    Qn::ResourceSavingFlags flags(QnWorkbenchLayout *layout) const;

    void setFlags(const QnLayoutResourcePtr &resource, Qn::ResourceSavingFlags flags);
    void setFlags(QnWorkbenchLayout *layout, Qn::ResourceSavingFlags flags);

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

    // TODO: #Elric move out?
    static bool isFile(const QnLayoutResourcePtr &resource);

signals:
    void flagsChanged(const QnLayoutResourcePtr &resource);

protected:
    void connectTo(const QnLayoutResourcePtr &resource);
    void disconnectFrom(const QnLayoutResourcePtr &resource);

    void processSaveLayoutsReply(int status, const QnLayoutResourceList &localLayouts, const QnLayoutResourceList &receivedLayouts);

    Qn::ResourceSavingFlags defaultFlags(const QnLayoutResourcePtr &resource) const;

    ec2::AbstractECConnectionPtr connection2() const;

protected slots:
    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);
    void at_layout_storeRequested(const QnLayoutResourcePtr &resource);
    void at_layout_changed(const QnLayoutResourcePtr &resource);
    void at_layout_changed(const QnResourcePtr &resource);

private:
    friend class QnWorkbenchLayoutReplyProcessor;

    /** Layout state storage that this object manages. */
    QnWorkbenchLayoutSnapshotStorage *m_storage;

    /** Layout to flags mapping. */
    QHash<QnLayoutResourcePtr, Qn::ResourceSavingFlags> m_flagsByLayout;
};


#endif // QN_WORKBENCH_LAYOUT_SNAPSHOT_MANAGER_H
