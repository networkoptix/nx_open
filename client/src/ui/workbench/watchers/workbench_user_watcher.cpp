#include "workbench_user_watcher.h"

#include <client/client_settings.h>

#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/actions/action_manager.h>

QnWorkbenchUserWatcher::QnWorkbenchUserWatcher(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(resourcePool(), &QnResourcePool::resourceAdded,     this,   &QnWorkbenchUserWatcher::at_resourcePool_resourceAdded);
    connect(resourcePool(), &QnResourcePool::resourceRemoved,   this,   &QnWorkbenchUserWatcher::at_resourcePool_resourceRemoved);
    foreach(const QnResourcePtr &resource, resourcePool()->getResources())
        at_resourcePool_resourceAdded(resource);
}

QnWorkbenchUserWatcher::~QnWorkbenchUserWatcher() {
    while(!m_users.empty())
        at_resourcePool_resourceRemoved(m_users.back());
}

void QnWorkbenchUserWatcher::setCurrentUser(const QnUserResourcePtr &user) {
    if(m_user == user)
        return;

    if (m_user)
        disconnect(m_user, NULL, this, NULL);

    m_user = user;
    m_userPassword = QString();
    m_userPasswordHash = user ? user->getHash() : QByteArray();

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

    foreach(const QnUserResourcePtr &user, m_users)
        at_resource_nameChanged(user);
}

void QnWorkbenchUserWatcher::setUserPassword(const QString &password) {
    if (m_userPassword == password)
        return;

    m_userPassword = password;
    m_userPasswordHash = QByteArray(); //hash will be recalculated
}

void QnWorkbenchUserWatcher::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
    if(!user)
        return;

    m_users.push_back(user);
    connect(user.data(), SIGNAL(nameChanged(const QnResourcePtr &)), this, SLOT(at_resource_nameChanged(const QnResourcePtr &)));

    at_resource_nameChanged(user);
}

void QnWorkbenchUserWatcher::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) {
    QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
    if(!user)
        return;

    disconnect(user.data(), NULL, this, NULL);
    m_users.removeOne(user);

    if(user == m_user)
        setCurrentUser(QnUserResourcePtr()); /* Assume there are no users with duplicate names. */
}

void QnWorkbenchUserWatcher::at_resource_nameChanged(const QnResourcePtr &resource) {
    QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
    if(!user)
        return;

    /* Logins are case-insensitive. */
    if(user->getName().toLower() == m_userName.toLower()) {
        setCurrentUser(user);
    } else if(user == m_user) {
        setCurrentUser(QnUserResourcePtr());
    }
}

bool QnWorkbenchUserWatcher::reconnectRequired(const QnUserResourcePtr &user) {
    if (user->getName() != m_userName)
        return true;

    if (m_userPassword.isEmpty())
        return m_userPasswordHash != user->getHash();

    // password was just changed by the user
    QList<QByteArray> values =  user->getHash().split(L'$');
    if (values.size() != 3)
        return true;

    QByteArray salt = values[1];
    QCryptographicHash md5(QCryptographicHash::Md5);
    md5.addData(salt);
    md5.addData(m_userPassword.toUtf8());
    if (md5.result().toHex() != values[2])
        return true;

    m_userPasswordHash = user->getHash();
    m_userPassword = QString();
    return false;
}

void QnWorkbenchUserWatcher::at_user_resourceChanged(const QnResourcePtr &resource) {
    if (!reconnectRequired(resource.dynamicCast<QnUserResource>()))
        return;

    //TODO: #Elric #TR add message box on next step of translations generating
    menu()->trigger(Qn::DisconnectAction,
        QnActionParameters().withArgument(Qn::ForceRole, true));
    menu()->trigger(Qn::ConnectToServerAction);

}

void QnWorkbenchUserWatcher::at_user_permissionsChanged(const QnUserResourcePtr &user) {
    Q_UNUSED(user)
    menu()->trigger(Qn::ReconnectAction);
}