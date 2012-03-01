#ifndef QN_WORKBENCH_LAYOUT_SNAPSHOT_MANAGER_H
#define QN_WORKBENCH_LAYOUT_SNAPSHOT_MANAGER_H

#include <QObject>
#include <core/resource/resource_fwd.h>
#include <api/AppServerConnection.h>

class QnWorkbenchContext;
class QnWorkbenchLayoutSnapshotStorage;
class QnWorkbenchLayoutSnapshotManager;

namespace detail {
    class QnWorkbenchLayoutReplyProcessor: public QObject {
        Q_OBJECT

    public:
        QnWorkbenchLayoutReplyProcessor(QnWorkbenchLayoutSnapshotManager *manager, const QnLayoutResourcePtr &resource): 
            m_manager(manager),
            m_resource(resource)
        {}

    public slots:
        void at_finished(int status, const QByteArray &errorString, const QnResourceList &resources, int handle);

    signals:
        void finished(int status, const QByteArray &errorString, const QnLayoutResourcePtr &resource);

    private:
        QWeakPointer<QnWorkbenchLayoutSnapshotManager> m_manager;
        QnLayoutResourcePtr m_resource;
    };

} // namespace detail


class QnWorkbenchLayoutSnapshotManager: public QObject {
    Q_OBJECT;
public:
    enum LayoutFlag {
        /** Layout is local and was never saved to appserver. */
        Local = 0x1,

        /** Layout is currently being saved to appserver. */
        BeingSaved = 0x2,

        /** Local unsaved changes are present in the layout. */
        Changed = 0x4,
    };
    Q_DECLARE_FLAGS(LayoutFlags, LayoutFlag);


    QnWorkbenchLayoutSnapshotManager(QObject *parent = NULL);

    virtual ~QnWorkbenchLayoutSnapshotManager();

    QnWorkbenchContext *context() const {
        return m_context;
    }

    void setContext(QnWorkbenchContext *context);

    void save(const QnLayoutResourcePtr &resource, QObject *object, const char *slot);

    void restore(const QnLayoutResourcePtr &resource);

    LayoutFlags flags(const QnLayoutResourcePtr &resource) const;

    bool isChanged(const QnLayoutResourcePtr &resource) const {
        return flags(resource) & Changed;
    }

    bool isLocal(const QnLayoutResourcePtr &resource) const {
        return flags(resource) & Local;
    }

    bool isBeingSaved(const QnLayoutResourcePtr &resource) const {
        return flags(resource) & BeingSaved;
    }

signals:
    void flagsChanged(const QnLayoutResourcePtr &resource);

protected:
    void start();
    void stop();

    QnLayoutResourceList poolLayoutResources() const;

    void setFlags(const QnLayoutResourcePtr &resource, LayoutFlags flags);

protected slots:
    void at_context_aboutToBeDestroyed();
    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);
    void at_layout_saved(const QnLayoutResourcePtr &resource);

private:
    friend class detail::QnWorkbenchLayoutReplyProcessor;

    /** Associated context. */
    QnWorkbenchContext *m_context;

    /** Layout state storage that this object manages. */
    QnWorkbenchLayoutSnapshotStorage *m_storage;

    /** Layout to flags mapping. */
    QHash<QnLayoutResourcePtr, LayoutFlags> m_flagsByLayout;

    /** Appserver connection. */
    QnAppServerConnectionPtr m_connection;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(QnWorkbenchLayoutSnapshotManager::LayoutFlags);

#endif // QN_WORKBENCH_LAYOUT_SNAPSHOT_MANAGER_H
