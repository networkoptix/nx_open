// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QObject>

#include <core/resource/resource_fwd.h>
#include <ui/workbench/workbench_context_aware.h>
#include <ui/workbench/workbench_state_manager.h>

namespace nx::vms::client::desktop {

/**
 * This class can be used for watching the resource pool for a user resource
 * that has a specific name.
 *
 * Once such resource is inserted into the pool, <tt>userChanged</tt> signal
 * is emitted. If its name changes, or if it is deleted from the pool,
 * <tt>userChanged</tt> signal will be emitted again, passing a null resource.
 *
 * Implements logic for automatic reconnect on user password change.
 * Automatic reconnect can be prevented by setReconnectOnPasswordChange(false) call.
 */
class ContextCurrentUserWatcher: public QObject, public QnWorkbenchContextAware
{
    Q_OBJECT
    using base_type = QObject;

public:
    ContextCurrentUserWatcher(QObject* parent = nullptr);
    virtual ~ContextCurrentUserWatcher() override;

    void setUserPassword(const QString &password);
    const QString &userPassword() const;

    void setReconnectOnPasswordChange(bool reconnect);

private:
    void at_user_resourceChanged(const QnResourcePtr &resource);

private:
    void setCurrentUser(const QnUserResourcePtr &currentUser);
    bool isReconnectRequired(const QnUserResourcePtr &user);
    void reconnect();

    QSet<QnUuid> allParentGroups() const;
    bool updateParentGroups();

private:
    QString m_userName;
    QString m_userPassword;
    QByteArray m_userDigest;
    QnUserResourcePtr m_user;
    bool m_reconnectOnPasswordChange = true;
    QSet<QnUuid> m_allParentGroups;
};

} // namespace nx::vms::client::desktop
