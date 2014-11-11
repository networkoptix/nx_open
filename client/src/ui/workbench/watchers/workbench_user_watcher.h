#ifndef QN_WORKBENCH_USER_WATCHER_H
#define QN_WORKBENCH_USER_WATCHER_H

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>

#include <utils/common/connective.h>

class QnWorkbenchPermissionsNotifier;

/**
 * This class can be used for watching the resource pool for a user resource
 * that has a specific name.
 *
 * Once such resource is inserted into the pool, <tt>userChanged</tt> signal
 * is emitted. If its name changes, or if it is deleted from the pool,
 * <tt>userChanged</tt> signal will be emitted again, passing a null resource.
 */
class QnWorkbenchUserWatcher: public Connective<QObject>, public QnWorkbenchContextAware {
    Q_OBJECT

    typedef Connective<QObject> base_type;
public:
    QnWorkbenchUserWatcher(QObject *parent = NULL);

    virtual ~QnWorkbenchUserWatcher();

    void setUserName(const QString &name);
    const QString &userName() const {
        return m_userName;
    }

    void setUserPassword(const QString &password);
    const QString &userPassword() const {
        return m_userPassword;
    }

    const QnUserResourcePtr &user() const {
        return m_user;
    }

signals:
    void userChanged(const QnUserResourcePtr &user);
    void reconnectRequired();

private slots:
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);

    void at_user_resourceChanged(const QnResourcePtr &resource);
    void at_user_permissionsChanged(const QnResourcePtr &user);
private:
    void setCurrentUser(const QnUserResourcePtr &currentUser);
    bool isReconnectRequired(const QnUserResourcePtr &user);

private:
    QString m_userName;
    QString m_userPassword;
    QByteArray m_userPasswordHash;
    Qn::Permissions m_userPermissions;
    QPointer<QnWorkbenchPermissionsNotifier> m_permissionsNotifier;
    QnUserResourcePtr m_user;
};

#endif // QN_WORKBENCH_USER_WATCHER_H
