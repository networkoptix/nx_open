#ifndef QN_RESOURCE_POOL_USER_WATCHER_H
#define QN_RESOURCE_POOL_USER_WATCHER_H

#include <QObject>
#include <core/resource/resource_fwd.h>

class QnResourcePool;

/**
 * This class can be used for watching the resource pool for a user resource 
 * that has a specific name. 
 * 
 * Once such resource is inserted into the pool, <tt>userChanged</tt> signal 
 * is emitted. If its name changes, or if it is deleted from the pool, 
 * <tt>userChanged</tt> signal will be emitted again, passing a null resource.
 */
class QN_EXPORT QnResourcePoolUserWatcher: public QObject {
    Q_OBJECT;
public:
    QnResourcePoolUserWatcher(QnResourcePool *resourcePool, QObject *parent = NULL);

    virtual ~QnResourcePoolUserWatcher();

    void setUserName(const QString &name);

    const QString &userName() const {
        return m_userName;
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
    QnResourcePool *m_resourcePool;
    QnUserResourceList m_users;
    QString m_userName;
    QnUserResourcePtr m_user;
};

#endif // QN_RESOURCE_POOL_USER_WATCHER_H
