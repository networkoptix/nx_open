#include "resource_pool_user_watcher.h"
#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>
#include <core/resource/user_resource.h>
#include "resource_pool.h"

QnResourcePoolUserWatcher::QnResourcePoolUserWatcher(QnResourcePool *resourcePool, QObject *parent):
    QObject(parent),
    m_resourcePool(resourcePool)
{
    static volatile bool metaTypesInitialized = false;
    if (!metaTypesInitialized) {
        qRegisterMetaType<QnUserResourcePtr>();

        metaTypesInitialized = true;
    }

    if(resourcePool == NULL) {
        qnNullWarning(resourcePool);
        return;
    }

    connect(m_resourcePool, SIGNAL(resourceAdded(const QnResourcePtr &)), this, SLOT(at_resourcePool_resourceAdded(const QnResourcePtr &)));
    connect(m_resourcePool, SIGNAL(resourceRemoved(const QnResourcePtr &)), this, SLOT(at_resourcePool_resourceRemoved(const QnResourcePtr &)));
    foreach(const QnResourcePtr &resource, m_resourcePool->getResources())
        at_resourcePool_resourceAdded(resource);
}

QnResourcePoolUserWatcher::~QnResourcePoolUserWatcher() {
    while(!m_users.empty())
        at_resourcePool_resourceRemoved(m_users.back());
}

void QnResourcePoolUserWatcher::setCurrentUser(const QnUserResourcePtr &user) {
    if(m_user == user)
        return;

    m_user = user;

    emit userChanged(user);
}

void QnResourcePoolUserWatcher::setUserName(const QString &name) {
    if(m_userName == name)
        return;

    m_userName = name;

    foreach(const QnUserResourcePtr &user, m_users)
        at_userResource_nameChanged(user);
}

void QnResourcePoolUserWatcher::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
    if(!user)
        return;

    m_users.push_back(user);
    connect(user.data(), SIGNAL(nameChanged()), this, SLOT(at_userResource_nameChanged()));

    at_userResource_nameChanged(user);
}

void QnResourcePoolUserWatcher::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) {
    QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
    if(!user)
        return;

    disconnect(user.data(), SIGNAL(nameChanged()), this, SLOT(at_userResource_nameChanged()));
    m_users.removeOne(user);

    if(user == m_user)
        setCurrentUser(QnUserResourcePtr()); /* Assume there are no users with duplicate names. */
}

void QnResourcePoolUserWatcher::at_userResource_nameChanged(const QnUserResourcePtr &user) {
    if(user->getName() == m_userName) {
        setCurrentUser(user);
    } else if(user == m_user) {
        setCurrentUser(QnUserResourcePtr());
    }
}

void QnResourcePoolUserWatcher::at_userResource_nameChanged() {
    at_userResource_nameChanged(toSharedPointer(checked_cast<QnUserResource *>(sender())));
}

