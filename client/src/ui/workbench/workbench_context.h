#ifndef QN_WORKBENCH_CONTEXT_H
#define QN_WORKBENCH_CONTEXT_H

#include <QObject>
#include <core/resource/resource_fwd.h>

class QnResourcePoolUserWatcher;
class QnResourcePool;
class QnWorkbench;

class QnWorkbenchContext: public QObject {
    Q_OBJECT;
public:
    QnWorkbenchContext(QnResourcePool *resourcePool, QObject *parent = NULL);

    virtual ~QnWorkbenchContext();

    static QnWorkbenchContext *instance();

    QnResourcePool *resourcePool() const {
        return m_resourcePool;
    }

    QnWorkbench *workbench() const {
        return m_workbench;
    }

    QnUserResourcePtr user();

signals:
    void userChanged(const QnUserResourcePtr &user);
    void aboutToBeDestroyed();

protected slots:
    void at_settings_lastUsedConnectionChanged();
    void at_resourcePool_aboutToBeDestroyed();

private:
    QnResourcePoolUserWatcher *m_userWatcher;
    QnResourcePool *m_resourcePool;
    QnWorkbench *m_workbench;
};


#define qnContext (QnWorkbenchContext::instance())


#endif // QN_WORKBENCH_CONTEXT_H
