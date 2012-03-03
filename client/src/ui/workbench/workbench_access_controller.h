#ifndef QN_WORKBENCH_ACCESS_CONTROLLER_H
#define QN_WORKBENCH_ACCESS_CONTROLLER_H

#include <QObject>
#include <core/resource/resource_fwd.h>

class QnWorkbenchContext;
class QnResourcePool;

class QnWorkbenchAccessController: public QObject {
    Q_OBJECT;
public:
    QnWorkbenchAccessController(QObject *parent = NULL);

    virtual ~QnWorkbenchAccessController();

    void setContext(QnWorkbenchContext *context);

    QnWorkbenchContext *context() const {
        return m_context;
    }

    QnResourcePool *resourcePool() const;

protected:
    void start();
    void stop();

    bool isAccessible(const QnUserResourcePtr &user);
    void updateAccessRights(const QnUserResourcePtr &user);
    void updateAccessRights(const QnUserResourceList &users);

protected slots:
    void at_context_aboutToBeDestroyed();
    void at_context_userChanged(const QnUserResourcePtr &user);
    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);

private:
    QnWorkbenchContext *m_context;
};


#endif // QN_WORKBENCH_ACCESS_CONTROLLER_H
