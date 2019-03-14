#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>

#include <ui/workbench/workbench_context_aware.h>
#include <ui/workbench/workbench_state_manager.h>

#include <utils/common/connective.h>

/**
 * This class can be used for watching the resource pool for a user resource
 * that has a specific name.
 *
 * Once such resource is inserted into the pool, <tt>userChanged</tt> signal
 * is emitted. If its name changes, or if it is deleted from the pool,
 * <tt>userChanged</tt> signal will be emitted again, passing a null resource.
 */
class QnWorkbenchUserWatcher: public Connective<QObject>, public QnSessionAwareDelegate
{
    Q_OBJECT

    typedef Connective<QObject> base_type;
public:
    QnWorkbenchUserWatcher(QObject *parent = NULL);

    virtual ~QnWorkbenchUserWatcher();

    /** Handle disconnect from server. */
    virtual bool tryClose(bool force) override;

    virtual void forcedUpdate() override;

    void setUserName(const QString &name);
    const QString &userName() const;

    void setUserPassword(const QString &password);
    const QString &userPassword() const;

    const QnUserResourcePtr &user() const;

    void setReconnectOnPasswordChange(bool reconnect);

signals:
    void userChanged(const QnUserResourcePtr &user);

private:
    void at_resourcePool_resourceRemoved(const QnResourcePtr &resource);

    void at_user_resourceChanged(const QnResourcePtr &resource);

private:
    void setCurrentUser(const QnUserResourcePtr &currentUser);
    bool isReconnectRequired(const QnUserResourcePtr &user);
    void reconnect();

    QnUserResourcePtr calculateCurrentUser() const;

private:
    QString m_userName;
    QString m_userPassword;
    QByteArray m_userDigest;
    QnUserResourcePtr m_user;
    bool m_reconnectOnPasswordChange;
};

