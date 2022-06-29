// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "context_current_user_watcher.h"

#include <client/client_message_processor.h>
#include <client/client_settings.h>
#include <common/common_module.h>
#include <core/resource/user_resource.h>
#include <core/resource_access/global_permissions_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <network/system_helpers.h>
#include <nx/vms/client/core/network/remote_connection.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <utils/common/checked_cast.h>

namespace nx::vms::client::desktop {

ContextCurrentUserWatcher::ContextCurrentUserWatcher(QObject *parent):
    base_type(parent),
    QnSessionAwareDelegate(parent)
{
    connect(qnClientMessageProcessor, &QnClientMessageProcessor::initialResourcesReceived, this,
        &ContextCurrentUserWatcher::forcedUpdate);

    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        &ContextCurrentUserWatcher::at_resourcePool_resourceRemoved);

    connect(globalPermissionsManager(), &QnGlobalPermissionsManager::globalPermissionsChanged,
        this,
        [this](const QnResourceAccessSubject& subject, GlobalPermissions /*value*/)
        {
            if (!subject.user() || subject.user() != m_user)
                return;

            /* We may get globalPermissionsChanged when user is removed. */
            if (resourcePool()->getResourceById(m_user->getId()))
                reconnect();
        });
}

ContextCurrentUserWatcher::~ContextCurrentUserWatcher() {}

bool ContextCurrentUserWatcher::tryClose(bool force)
{
    if (force)
        setUserName(QString());
    return true;
}

void ContextCurrentUserWatcher::forcedUpdate()
{
    setCurrentUser(calculateCurrentUser());
}

void ContextCurrentUserWatcher::setCurrentUser(const QnUserResourcePtr &user)
{
    if (m_user == user)
        return;

    if (m_user)
        m_user->disconnect(this);

    m_user = user;
    m_userPassword = QString();
    m_userDigest = QByteArray();

    if (m_user)
    {
        m_userDigest = m_user->getDigest();

        connect(m_user.get(), &QnResource::nameChanged, this,
            &ContextCurrentUserWatcher::at_user_resourceChanged);
        connect(m_user.get(), &QnUserResource::passwordChanged, this,
            &ContextCurrentUserWatcher::at_user_resourceChanged);
    }

    emit userChanged(user);
}

void ContextCurrentUserWatcher::setUserName(const QString &name)
{
    if (m_userName == name)
        return;
    m_userName = name;
    setCurrentUser(calculateCurrentUser());
}

const QString & ContextCurrentUserWatcher::userName() const
{
    return m_userName;
}

void ContextCurrentUserWatcher::setUserPassword(const QString &password)
{
    if (m_userPassword == password)
        return;

    m_userPassword = password;
    m_userDigest = QByteArray(); //hash will be recalculated
}

const QString & ContextCurrentUserWatcher::userPassword() const
{
    return m_userPassword;
}

const QnUserResourcePtr & ContextCurrentUserWatcher::user() const
{
    return m_user;
}

void ContextCurrentUserWatcher::setReconnectOnPasswordChange(bool value)
{
    m_reconnectOnPasswordChange = value;
    if (value && m_user && isReconnectRequired(m_user))
        reconnect();
}

QnUserResourcePtr ContextCurrentUserWatcher::calculateCurrentUser() const
{
    for (const auto& user: resourcePool()->getResources<QnUserResource>())
    {
        if (user->getName().toLower() != m_userName.toLower())
            continue;
        return user;
    }
    return QnUserResourcePtr();
}

void ContextCurrentUserWatcher::at_resourcePool_resourceRemoved(const QnResourcePtr &resource)
{
    QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
    if (!user || user != m_user)
        return;

    auto currentConnection = this->connection();
    if (!currentConnection)
        return;

    setCurrentUser(QnUserResourcePtr());
    if (!currentConnection->moduleInformation().id.isNull())
    {
        menu()->trigger(ui::action::DisconnectAction, {Qn::ForceRole, true});
    }
}

bool ContextCurrentUserWatcher::isReconnectRequired(const QnUserResourcePtr &user)
{
    if (m_userName.isEmpty())
        return false;   //we are not connected

    // TODO: #sivanov Actually reconnect on renaming is required when using digest auth only. But
    // some functionality (like desktop camera) can stop working, so it's simplier to reconnect.
    if (user->getName().toLower() != m_userName.toLower())
        return true;

    // TODO: #sivanov This is needed to avoid reconnecting on merging systems. Should be improved.
    if (!m_reconnectOnPasswordChange)
        return false;

    const bool bearerAuthIsUsed =
        (connectionCredentials().authToken.type == nx::network::http::AuthTokenType::bearer);

    // Reconnect is never needed when token auth is used.
    if (bearerAuthIsUsed)
    {
        m_userDigest = user->getDigest();
        m_userPassword = QString();
        return false;
    }

    if (m_userPassword.isEmpty())
        return m_userDigest != user->getDigest();

    // TODO: #sivanov Handle this on the server side: force disconnect users with changed password.
    // Filled m_userPassword means password was just changed by the user.
    if (!user->getHash().checkPassword(m_userPassword))
        return true;

    m_userDigest = user->getDigest();
    m_userPassword = QString();
    return false;
}

void ContextCurrentUserWatcher::reconnect()
{
    menu()->trigger(ui::action::ReconnectAction);
}

void ContextCurrentUserWatcher::at_user_resourceChanged(const QnResourcePtr &resource)
{
    if (isReconnectRequired(resource.dynamicCast<QnUserResource>()))
        reconnect();
}

} // namespace nx::vms::client::desktop
