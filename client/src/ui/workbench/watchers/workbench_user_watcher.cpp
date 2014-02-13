#include "workbench_user_watcher.h"

#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>

QnWorkbenchUserWatcher::QnWorkbenchUserWatcher(QObject *parent):
    QObject(parent),
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

    m_user = user;

    emit userChanged(user);
}

void QnWorkbenchUserWatcher::setUserName(const QString &name) {
    if(m_userName == name)
        return;

    m_userName = name;

    foreach(const QnUserResourcePtr &user, m_users)
        at_resource_nameChanged(user);
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

