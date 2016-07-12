#include "workbench_user_watcher.h"

#include <common/common_module.h>

#include <client/client_settings.h>
#include <client/client_message_processor.h>

#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource_management/resource_access_manager.h>

#include <ui/actions/action_manager.h>

#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>

QnWorkbenchUserWatcher::QnWorkbenchUserWatcher(QObject *parent):
    base_type(parent),
    QnWorkbenchStateDelegate(parent),
    m_reconnectOnPasswordChange(true)
{
    connect(QnClientMessageProcessor::instance(), &QnClientMessageProcessor::initialResourcesReceived, this, &QnWorkbenchUserWatcher::forcedUpdate);

    connect(qnResPool, &QnResourcePool::resourceRemoved, this, &QnWorkbenchUserWatcher::at_resourcePool_resourceRemoved);
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
        connect(m_user, &QnResource::resourceChanged, this, &QnWorkbenchUserWatcher::at_user_resourceChanged); //TODO: #GDM #Common get rid of resourceChanged
        connect(m_user, &QnUserResource::permissionsChanged, this, &QnWorkbenchUserWatcher::at_user_permissionsChanged);
        connect(m_user, &QnUserResource::userGroupChanged, this, &QnWorkbenchUserWatcher::at_user_permissionsChanged);
        connect(m_user, &QnUserResource::enabledChanged, this, &QnWorkbenchUserWatcher::at_user_permissionsChanged);
        connect(m_user, &QnUserResource::permissionsChanged, this, &QnWorkbenchUserWatcher::at_user_permissionsChanged);

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

void QnWorkbenchUserWatcher::setReconnectOnPasswordChange(bool reconnect)
{
    m_reconnectOnPasswordChange = reconnect;
    if (reconnect && m_user && isReconnectRequired(m_user))
        emit reconnectRequired();
}


QnUserResourcePtr QnWorkbenchUserWatcher::calculateCurrentUser() const
{
    for (const QnUserResourcePtr &user : qnResPool->getResources<QnUserResource>())
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
    if (!qnCommon->remoteGUID().isNull())
        menu()->trigger(QnActions::DisconnectAction, QnActionParameters().withArgument(Qn::ForceRole, true));
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
    if (!user->checkPassword(m_userPassword))
        return true;

    m_userDigest = user->getDigest();
    m_userPassword = QString();
    return false;
}

void QnWorkbenchUserWatcher::at_user_resourceChanged(const QnResourcePtr &resource)
{
    if (!isReconnectRequired(resource.dynamicCast<QnUserResource>()))
        return;
    emit reconnectRequired();
}

void QnWorkbenchUserWatcher::at_user_permissionsChanged(const QnResourcePtr &user)
{
    if (!m_user || user.dynamicCast<QnUserResource>() != m_user)
        return;
    emit reconnectRequired();
}

