#include "workbench_user_watcher.h"

#include <common/common_module.h>

#include <client/client_settings.h>
#include <client/client_message_processor.h>

#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/actions/action_manager.h>
#include <ui/workbench/workbench_access_controller.h>

#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>

QnWorkbenchUserWatcher::QnWorkbenchUserWatcher(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_reconnectOnPasswordChange(true)
{
    connect(QnClientMessageProcessor::instance(),   &QnClientMessageProcessor::initialResourcesReceived,    this,   [this] {
        setCurrentUser(calculateCurrentUser());
    });

    connect(resourcePool(), &QnResourcePool::resourceRemoved,   this,   &QnWorkbenchUserWatcher::at_resourcePool_resourceRemoved);
}

QnWorkbenchUserWatcher::~QnWorkbenchUserWatcher() {}

void QnWorkbenchUserWatcher::setCurrentUser(const QnUserResourcePtr &user) {
    if(m_user == user)
        return;

    if (m_user) {
        disconnect(m_user, NULL, this, NULL);
        if (m_permissionsNotifier)
            disconnect(m_permissionsNotifier, NULL, this, NULL);
    }

    m_user = user;
    m_userPassword = QString();
    m_userDigest = user ? user->getDigest() : QByteArray();
    m_userPermissions = accessController()->globalPermissions(user);
    m_permissionsNotifier = user ? accessController()->notifier(user) : NULL;

    if (m_user) {
        connect(m_user, &QnResource::resourceChanged, this, &QnWorkbenchUserWatcher::at_user_resourceChanged); //TODO: #GDM #Common get rid of resourceChanged

        connect(m_user, &QnUserResource::permissionsChanged, this, &QnWorkbenchUserWatcher::at_user_permissionsChanged);
    }


    emit userChanged(user);
}

void QnWorkbenchUserWatcher::setUserName(const QString &name) {
    if(m_userName == name)
        return;
    m_userName = name;
    setCurrentUser(calculateCurrentUser());
}

void QnWorkbenchUserWatcher::setUserPassword(const QString &password) {
    if (m_userPassword == password)
        return;

    m_userPassword = password;
    m_userDigest = QByteArray(); //hash will be recalculated
}

void QnWorkbenchUserWatcher::setReconnectOnPasswordChange(bool reconnect) {
    m_reconnectOnPasswordChange = reconnect;
    if (reconnect && m_user && isReconnectRequired(m_user))
        emit reconnectRequired();
}


QnUserResourcePtr QnWorkbenchUserWatcher::calculateCurrentUser() const {
    for (const QnUserResourcePtr &user: qnResPool->getResources<QnUserResource>()) {
        if ( user->getName().toLower() != m_userName.toLower() )
            continue;
        return user;
    }
    return QnUserResourcePtr();
}

void QnWorkbenchUserWatcher::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) {
    QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
    if(!user || user != m_user)
        return;

    setCurrentUser(QnUserResourcePtr());
    if (!qnCommon->remoteGUID().isNull())
        menu()->trigger(QnActions::DisconnectAction, QnActionParameters().withArgument(Qn::ForceRole, true));
}

bool QnWorkbenchUserWatcher::isReconnectRequired(const QnUserResourcePtr &user) {
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

void QnWorkbenchUserWatcher::at_user_resourceChanged(const QnResourcePtr &resource) {
    if (!isReconnectRequired(resource.dynamicCast<QnUserResource>()))
        return;
    emit reconnectRequired();
}

void QnWorkbenchUserWatcher::at_user_permissionsChanged(const QnResourcePtr &user) {
    if (!m_user || user.dynamicCast<QnUserResource>() != m_user)
        return;

    Qn::GlobalPermissions newPermissions = accessController()->globalPermissions(m_user);
    /* Reconnect if some permissions were changed*/
    const bool reconnect = (newPermissions != m_userPermissions);
    m_userPermissions = newPermissions;
    if (reconnect)
        emit reconnectRequired();
}

