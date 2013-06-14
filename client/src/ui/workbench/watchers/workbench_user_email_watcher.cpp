#include "workbench_user_email_watcher.h"

#include <core/resource/user_resource.h>
#include <core/resource_managment/resource_pool.h>

#include <utils/common/email.h>

QnWorkbenchUserEmailWatcher::QnWorkbenchUserEmailWatcher(QObject *parent) :
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
    connect(resourcePool(), SIGNAL(resourceAdded(const QnResourcePtr &)),   this,   SLOT(at_resourcePool_resourceAdded(const QnResourcePtr &)));
    connect(resourcePool(), SIGNAL(resourceRemoved(const QnResourcePtr &)), this,   SLOT(at_resourcePool_resourceRemoved(const QnResourcePtr &)));
    foreach(const QnResourcePtr &resource, resourcePool()->getResources())
        at_resourcePool_resourceAdded(resource);
}

QnWorkbenchUserEmailWatcher::~QnWorkbenchUserEmailWatcher() {
    while(!m_emailValidByUser.empty())
        at_resourcePool_resourceRemoved(m_emailValidByUser.keys().back());
}

void QnWorkbenchUserEmailWatcher::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
    if(!user)
        return;

    connect(user.data(), SIGNAL(emailChanged(const QnUserResourcePtr &)), this, SLOT(at_user_emailChanged(const QnUserResourcePtr &)));

    at_user_emailChanged(user);
}

void QnWorkbenchUserEmailWatcher::at_resourcePool_resourceRemoved(const QnResourcePtr &resource) {
    QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
    if(!user)
        return;

    disconnect(user.data(), NULL, this, NULL);

    /* Any email is valid for removed user. */
    if (!m_emailValidByUser[user])
        emit userEmailValidityChanged(user, true);
    m_emailValidByUser.remove(user);
}

void QnWorkbenchUserEmailWatcher::at_user_emailChanged(const QnUserResourcePtr &user) {
    bool valid = QnEmail::isValid(user->getEmail());
    if (m_emailValidByUser.contains(user) && m_emailValidByUser[user] == valid)
        return;

    m_emailValidByUser[user] = valid;
    emit userEmailValidityChanged(user, valid);
}
