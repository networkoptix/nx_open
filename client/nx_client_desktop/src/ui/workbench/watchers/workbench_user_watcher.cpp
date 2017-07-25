#include "workbench_user_watcher.h"

#include <common/common_module.h>

#include <client/client_settings.h>
#include <client/client_message_processor.h>

#include <core/resource_access/global_permissions_manager.h>

#include <core/resource_management/resource_pool.h>

#include <core/resource/user_resource.h>

#include <nx/client/desktop/ui/actions/action_manager.h>

#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>

using namespace nx::client::desktop::ui;

QnWorkbenchUserWatcher::QnWorkbenchUserWatcher(QObject *parent):
    base_type(parent),
    QnSessionAwareDelegate(parent),
    m_userName(),
    m_userPassword(),
    m_userDigest(),
    m_user(),
    m_reconnectOnPasswordChange(true)
{
    connect(qnClientMessageProcessor, &QnClientMessageProcessor::initialResourcesReceived, this,
        &QnWorkbenchUserWatcher::forcedUpdate);

    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        &QnWorkbenchUserWatcher::at_resourcePool_resourceRemoved);

    connect(globalPermissionsManager(), &QnGlobalPermissionsManager::globalPermissionsChanged,
        this,
        [this](const QnResourceAccessSubject& subject, Qn::GlobalPermissions /*value*/)
        {
            if (!subject.user() || subject.user() != m_user)
                return;

            /* We may get globalPermissionsChanged when user is removed. */
            if (m_user->resourcePool())
                reconnect();
        });
}

QnWorkbenchUserWatcher::~QnWorkbenchUserWatcher() {}

bool QnWorkbenchUserWatcher::tryClose(bool force)
{
    if (force)
        setUserName(QString());
    return true;
}

void QnWorkbenchUserWatcher::forcedUpdate()
{
    setCurrentUser(calculateCurrentUser());
}

void QnWorkbenchUserWatcher::setCurrentUser(const QnUserResourcePtr &user)
{
    if (m_user == user)
        return;

    if (m_user)
        disconnect(m_user, NULL, this, NULL);

    m_user = user;
    m_userPassword = QString();
    m_userDigest = QByteArray();

    if (m_user)
    {
        m_userDigest = m_user->getDigest();

        // TODO: #GDM #Common get rid of resourceChanged
        connect(m_user, &QnResource::resourceChanged, this,
            &QnWorkbenchUserWatcher::at_user_resourceChanged);
    }

    emit userChanged(user);
}

void QnWorkbenchUserWatcher::setUserName(const QString &name)
{
    if (m_userName == name)
        return;
    m_userName = name;
    setCurrentUser(calculateCurrentUser());
}

const QString & QnWorkbenchUserWatcher::userName() const
{
    return m_userName;
}

void QnWorkbenchUserWatcher::setUserPassword(const QString &password)
{
    if (m_userPassword == password)
        return;

    m_userPassword = password;
    m_userDigest = QByteArray(); //hash will be recalculated
}

const QString & QnWorkbenchUserWatcher::userPassword() const
{
    return m_userPassword;
}

const QnUserResourcePtr & QnWorkbenchUserWatcher::user() const
{
    return m_user;
}

void QnWorkbenchUserWatcher::setReconnectOnPasswordChange(bool value)
{
    m_reconnectOnPasswordChange = value;
    if (value && m_user && isReconnectRequired(m_user))
        reconnect();
}


QnUserResourcePtr QnWorkbenchUserWatcher::calculateCurrentUser() const
{
    for (const QnUserResourcePtr &user : resourcePool()->getResources<QnUserResource>())
    {
        if (user->getName().toLower() != m_userName.toLower())
            continue;
        return user;
    }
    return QnUserResourcePtr();
}

void QnWorkbenchUserWatcher::at_resourcePool_resourceRemoved(const QnResourcePtr &resource)
{
    QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
    if (!user || user != m_user)
        return;

    setCurrentUser(QnUserResourcePtr());
    if (!commonModule()->remoteGUID().isNull())
    {
        menu()->trigger(action::DisconnectAction, {Qn::ForceRole, true});
    }
}

bool QnWorkbenchUserWatcher::isReconnectRequired(const QnUserResourcePtr &user)
{
    if (m_userName.isEmpty())
        return false;   //we are not connected

    if (user->getName().toLower() != m_userName.toLower())
        return true;

    if (!m_reconnectOnPasswordChange)
        return false;

    if (m_userPassword.isEmpty())
        return m_userDigest != user->getDigest();

    // password was just changed by the user
    if (!user->checkLocalUserPassword(m_userPassword))
        return true;

    m_userDigest = user->getDigest();
    m_userPassword = QString();
    return false;
}

void QnWorkbenchUserWatcher::reconnect()
{
    menu()->trigger(action::ReconnectAction);
}

void QnWorkbenchUserWatcher::at_user_resourceChanged(const QnResourcePtr &resource)
{
    if (isReconnectRequired(resource.dynamicCast<QnUserResource>()))
        reconnect();
}
