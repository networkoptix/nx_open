#ifndef QN_WORKBENCH_LAYOUT_MANAGER_H
#define QN_WORKBENCH_LAYOUT_MANAGER_H

#include <QObject>
#include <core/resource/resource_fwd.h>

class QnWorkbenchContext;
class QnWorkbenchLayoutStateStorage;

namespace detail {
    class QnWorkbenchLayoutReplyProcessor: public QObject {
        Q_OBJECT

    public:
        QnWorkbenchLayoutReplyProcessor(QnWorkbenchLayoutStateStorage *stateStorage, const QnLayoutResourcePtr &resource): 
            m_synchronizer(synchronizer),
            m_resource(resource)
        {}

    public slots:
        void at_finished(int status, const QByteArray &errorString, const QnResourceList &resources, int handle);

    signals:
        void finished(int status, const QByteArray &errorString, const QnLayoutResourcePtr &resource);

    private:
        QWeakPointer<QnWorkbenchLayoutStateStorage> m_synchronizer;
        QnLayoutResourcePtr m_resource;
    };

} // namespace detail


class QnWorkbenchLayoutStateManager: public QObject {
    Q_OBJECT;

public:
    QnWorkbenchLayoutStateManager(QObject *parent = NULL);

    virtual ~QnWorkbenchLayoutStateManager();

    QnWorkbenchContext *context() const {
        return m_context;
    }

    void setContext(QnWorkbenchContext *context);

    void save(const QnLayoutResourcePtr &resource, QObject *object, const char *slot);

    void restore(const QnLayoutResourcePtr &resource);

    bool isChanged(const QnLayoutResourcePtr &resource);

    bool isLocal(const QnLayoutResourcePtr &resource);

protected:
    void start();
    void stop();

    QnLayoutResourceList poolLayoutResources() const;

    void setChanged(const QnLayoutResourcePtr &resource, bool changed);

protected slots:
    void at_context_aboutToBeDestroyed();
    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);

private:
    QnWorkbenchContext *m_context;
    QnWorkbenchLayoutStateStorage *m_stateStorage;
};


#endif // QN_WORKBENCH_LAYOUT_MANAGER_H
