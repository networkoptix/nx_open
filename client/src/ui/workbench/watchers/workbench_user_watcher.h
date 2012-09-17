#ifndef QN_WORKBENCH_USER_WATCHER_H
#define QN_WORKBENCH_USER_WATCHER_H

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

/**
 * This class can be used for watching the resource pool for a user resource 
 * that has a specific name. 
 * 
 * Once such resource is inserted into the pool, <tt>userChanged</tt> signal 
 * is emitted. If its name changes, or if it is deleted from the pool, 
 * <tt>userChanged</tt> signal will be emitted again, passing a null resource.
 */
class QnWorkbenchUserWatcher: public QObject, public QnWorkbenchContextAware {
    Q_OBJECT;
public:
    QnWorkbenchUserWatcher(QObject *parent = NULL);

    virtual ~QnWorkbenchUserWatcher();

    void setUserName(const QString &name);

    const QString &userName() const {
        return m_userName;
    }

    const QnUserResourcePtr &user() const {
        return m_user;
    }

signals:
    void userChanged(const QnUserResourcePtr &user);

private slots:
    void at_resourcePool_resourceAdded(const QnResourcePtr &resource);
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);
    void at_userResource_nameChanged(const QnUserResourcePtr &user);
    void at_userResource_nameChanged();

private:
    void setCurrentUser(const QnUserResourcePtr &currentUser);

private:
    QnUserResourceList m_users;
    QString m_userName;
    QnUserResourcePtr m_user;
};

#endif // QN_WORKBENCH_USER_WATCHER_H
