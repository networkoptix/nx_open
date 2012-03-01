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

    void setContext(QnWorkbenchContext *context);

public slots:
    void submit();

protected:
    void start();
    void stop();

protected slots:
    void at_context_aboutToBeDestroyed();
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);
    void at_workbench_layoutsChanged();

private:
    /** Associated context. */
    QnWorkbenchContext *m_context;

    /** Whether changes should be propagated from workbench to resources. */
    bool m_submit;

    /** Whether changes should be propagated from resources to workbench. */
    bool m_update;
};


#endif // QN_WORKBENCH_SYNCHRONIZER_H
