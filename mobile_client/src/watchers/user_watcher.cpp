#include "user_watcher.h"

#include <common/common_module.h>

#include <client/client_message_processor.h>

#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>

#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>

QnUserWatcher::QnUserWatcher(QObject *parent):
    base_type(parent)
{
    connect(QnClientMessageProcessor::instance(), &QnClientMessageProcessor::initialResourcesReceived, this, [this] {
        for (const QnUserResourcePtr &user: qnResPool->getResources<QnUserResource>()) {
            if (user->getName().toLower() != m_userName.toLower())
                continue;

            setCurrentUser(user);
            return;
        }
        setCurrentUser(QnUserResourcePtr());
    });

    connect(qnResPool, &QnResourcePool::resourceRemoved,   this,   &QnUserWatcher::at_resourcePool_resourceRemoved);
}

QnUserWatcher::~QnUserWatcher() {}

void QnUserWatcher::setCurrentUser(const QnUserResourcePtr &user) {
    if(m_user == user)
        return;

    m_user = user;

    emit userChanged(user);
}

void QnUserWatcher::setUserName(const QString &name) {
    if(m_userName == name)
        return;

    m_userName = name;
}

void QnUserWatcher::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) {
    QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
    if (!user || user != m_user)
        return;

    setCurrentUser(QnUserResourcePtr());
}
