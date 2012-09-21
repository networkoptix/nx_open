#include "workbench_user_watcher.h"

#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>

#include <core/resource/user_resource.h>
#include <core/resource_managment/resource_pool.h>

QnWorkbenchUserWatcher::QnWorkbenchUserWatcher(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
    static volatile bool metaTypesInitialized = false;
    if (!metaTypesInitialized) {
        qRegisterMetaType<QnUserResourcePtr>();

        metaTypesInitialized = true;
    }

    connect(resourcePool(), SIGNAL(resourceAdded(const QnResourcePtr &)),   this,   SLOT(at_resourcePool_resourceAdded(const QnResourcePtr &)));
    connect(resourcePool(), SIGNAL(resourceRemoved(const QnResourcePtr &)), this,   SLOT(at_resourcePool_resourceRemoved(const QnResourcePtr &)));
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
        at_userResource_nameChanged(user);
}

void QnWorkbenchUserWatcher::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
    if(!user)
        return;

    m_users.push_back(user);
    connect(user.data(), SIGNAL(nameChanged()), this, SLOT(at_userResource_nameChanged()));

    at_userResource_nameChanged(user);
}

void QnWorkbenchUserWatcher::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) {
    QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
    if(!user)
        return;

    disconnect(user.data(), SIGNAL(nameChanged()), this, SLOT(at_userResource_nameChanged()));
    m_users.removeOne(user);

    if(user == m_user)
        setCurrentUser(QnUserResourcePtr()); /* Assume there are no users with duplicate names. */
}

void QnWorkbenchUserWatcher::at_userResource_nameChanged(const QnUserResourcePtr &user) {
    if(user->getName() == m_userName) {
        setCurrentUser(user);
    } else if(user == m_user) {
        setCurrentUser(QnUserResourcePtr());
    }
}

void QnWorkbenchUserWatcher::at_userResource_nameChanged() {
    at_userResource_nameChanged(toSharedPointer(checked_cast<QnUserResource *>(sender())));
}

