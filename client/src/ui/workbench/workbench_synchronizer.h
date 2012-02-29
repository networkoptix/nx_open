#ifndef QN_WORKBENCH_SYNCHRONIZER_H
#define QN_WORKBENCH_SYNCHRONIZER_H

#include <QObject>
#include <core/resource/resource_fwd.h>
#include <core/resource/layout_resource.h>
#include <api/AppServerConnection.h>

class QnWorkbench;
class QnWorkbenchLayout;
class QnWorkbenchContext;
class QnWorkbenchSynchronizer;
class QnResourcePool;

namespace detail {
    class WorkbenchSynchronizerReplyProcessor : public QObject {
        Q_OBJECT

    public:
        WorkbenchSynchronizerReplyProcessor(QnWorkbenchSynchronizer *synchronizer, const QnLayoutResourcePtr &resource): 
            m_synchronizer(synchronizer),
            m_resource(resource)
        {}

    public slots:
        void at_finished(int status, const QByteArray &errorString, const QnResourceList &resources, int handle);

    signals:
        void finished(int status, const QByteArray &errorString, const QnLayoutResourcePtr &resource);

    private:
        QWeakPointer<QnWorkbenchSynchronizer> m_synchronizer;
        QnLayoutResourcePtr m_resource;
    };

    class LayoutData {
    public:
        LayoutData(const QnLayoutResourcePtr &resource):
            items(resource->getItems()),
            name(resource->getName())
        {}

        LayoutData() {}

        QnLayoutItemDataMap items;
        QString name;
    };

} // namespace detail


/**
 * This class performs bidirectional synchronization of instances of 
 * <tt>QnWorkbench</tt> and <tt>QnResourcePool</tt>.
 */
class QnWorkbenchSynchronizer: public QObject {
    Q_OBJECT;

public:
    QnWorkbenchSynchronizer(QObject *parent = NULL);

    virtual ~QnWorkbenchSynchronizer();

    QnWorkbenchContext *context() const {
        return m_context;
    }

    bool isRunning() const {
        return m_running;
    }

    void save(QnWorkbenchLayout *layout, QObject *object, const char *slot);

    void restore(QnWorkbenchLayout *layout);

    bool isChanged(const QnLayoutResourcePtr &resource);

    bool isLocal(const QnLayoutResourcePtr &resource);

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
    void setContext(QnWorkbenchContext *context);
    void update();
    void submit();

protected:
    void start();
    void stop();
    QnLayoutResourcePtr checkLayoutResource(QnWorkbenchLayout *layout);
    QnLayoutResourceList poolLayoutResources() const;

    const detail::LayoutData &savedState(const QnLayoutResourcePtr &resource);
    void setSavedState(const QnLayoutResourcePtr &resource, const detail::LayoutData &state);
    void removeSavedState(const QnLayoutResourcePtr &resource);

protected slots:
    void at_context_aboutToBeDestroyed();
    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);
    void at_workbench_layoutsChanged();

private:
    friend class detail::WorkbenchSynchronizerReplyProcessor;

    /** Whether this synchronizer is running. */
    bool m_running;

    /** Associated context. */
    QnWorkbenchContext *m_context;

    /** Whether changes should be propagated from workbench to resources. */
    bool m_submit;

    /** Whether changes should be propagated from resources to workbench. */
    bool m_update;

    /** Appserver connection. */
    QnAppServerConnectionPtr m_connection;

    /** Mapping from layout resource to its saved state. */
    QHash<QnLayoutResourcePtr, detail::LayoutData> m_savedDataByResource;
};


#endif // QN_WORKBENCH_SYNCHRONIZER_H
