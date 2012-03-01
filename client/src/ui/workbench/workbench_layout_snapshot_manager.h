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


class QnWorkbenchLayoutSnapshotManager: public QObject {
    Q_OBJECT;
public:
    QnWorkbenchLayoutSnapshotManager(QObject *parent = NULL);

    virtual ~QnWorkbenchLayoutSnapshotManager();

    QnWorkbenchContext *context() const {
        return m_context;
    }

    void setContext(QnWorkbenchContext *context);

    void save(const QnLayoutResourcePtr &resource, QObject *object, const char *slot);

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

signals:
    void flagsChanged(const QnLayoutResourcePtr &resource);

protected:
    void start();
    void stop();

    void setFlags(const QnLayoutResourcePtr &resource, Qn::LayoutFlags flags);

    void connectTo(const QnLayoutResourcePtr &resource);
    void disconnectFrom(const QnLayoutResourcePtr &resource);

protected slots:
    void at_context_aboutToBeDestroyed();
    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);
    void at_layout_saved(const QnLayoutResourcePtr &resource);
    void at_layout_changed(const QnLayoutResourcePtr &resource);
    void at_layout_changed();

private:
    friend class detail::QnWorkbenchLayoutReplyProcessor;

    /** Associated context. */
    QnWorkbenchContext *m_context;

    /** Layout state storage that this object manages. */
    QnWorkbenchLayoutSnapshotStorage *m_storage;

    /** Layout to flags mapping. */
    QHash<QnLayoutResourcePtr, Qn::LayoutFlags> m_flagsByLayout;

    /** Appserver connection. */
    QnAppServerConnectionPtr m_connection;
};


#endif // QN_WORKBENCH_LAYOUT_SNAPSHOT_MANAGER_H
