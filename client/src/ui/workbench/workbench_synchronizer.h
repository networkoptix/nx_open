#ifndef QN_WORKBENCH_SYNCHRONIZER_H
#define QN_WORKBENCH_SYNCHRONIZER_H

#include <QObject>
#include <core/resource/resource_fwd.h>
#include <core/resource/layout_resource.h>
#include <api/AppServerConnection.h>

class QnWorkbench;
class QnWorkbenchLayout;
class QnWorkbenchSynchronizer;

namespace detail {
    class WorkbenchSynchronizerReplyProcessor : public QObject {
        Q_OBJECT

    public:
        WorkbenchSynchronizerReplyProcessor(QnWorkbenchSynchronizer *synchronizer, const QnLayoutResourcePtr &resource): 
            m_synchronizer(synchronizer),
            m_resource(resource)
        {}

    public slots:
        void at_finished(int status, const QByteArray &errorString, QnResourceList resources, int handle);

    signals:
        void finished(int status, const QByteArray &errorString, const QnLayoutResourcePtr &resource);

    private:
        QWeakPointer<QnWorkbenchSynchronizer> m_synchronizer;
        QnLayoutResourcePtr m_resource;
    };

} // namespace detail

/**
 * This class performs bidirectional synchronization of instances of 
 * <tt>QnWorkbench</tt> and <tt>QnUserResource</tt>.
 */
class QnWorkbenchSynchronizer: public QObject {
    Q_OBJECT;

public:
    QnWorkbenchSynchronizer(QObject *parent = NULL);

    virtual ~QnWorkbenchSynchronizer();

    QnWorkbench *workbench() {
        return m_workbench;
    }

    const QnUserResourcePtr &user() const {
        return m_user;
    }

    bool isRunning() const {
        return m_running;
    }

    void save(QnWorkbenchLayout *layout, QObject *object, const char *slot);

    void restore(QnWorkbenchLayout *layout);

    bool isChanged(QnWorkbenchLayout *layout);

    bool isLocal(QnWorkbenchLayout *layout);

signals:
    /**
     * This signal is emitted whenever synchronization starts. For synchronization
     * to start, both workbench and user must be set.
     */
    void started();

    /**
     * This signal is emitted whenever synchronization stops.
     */
    void stopped();

public slots:
    void setWorkbench(QnWorkbench *workbench);
    void setUser(const QnUserResourcePtr &user);
    void update();
    void submit();

protected:
    void start();
    void stop();
    QnLayoutResourcePtr checkLayoutResource(QnWorkbenchLayout *layout);

protected slots:
    void at_user_resourceChanged();
    void at_workbench_aboutToBeDestroyed();
    void at_workbench_layoutsChanged();

private:
    friend class detail::WorkbenchSynchronizerReplyProcessor;

    /** Whether this synchronizer is running. */
    bool m_running;

    /** Associated workbench. */
    QnWorkbench *m_workbench;

    /** Associated user resource. */
    QnUserResourcePtr m_user;

    /** Whether changes should be propagated from workbench to resources. */
    bool m_submit;

    /** Whether changes should be propagated from resources to workbench. */
    bool m_update;

    /** Appserver connection. */
    QnAppServerConnectionPtr m_connection;

    /** Mapping from layout resource to its saved state. */
    QHash<QnLayoutResourcePtr, QnLayoutItemDataMap> m_savedItemsByResource;
};


#endif // QN_WORKBENCH_SYNCHRONIZER_H
