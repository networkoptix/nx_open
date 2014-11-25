#include "workbench_user_email_watcher.h"

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>

#include <utils/common/email.h>

#include <ui/workbench/workbench_context.h>

QnWorkbenchUserEmailWatcher::QnWorkbenchUserEmailWatcher(QObject *parent) :
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(resourcePool(), SIGNAL(resourceAdded(const QnResourcePtr &)),   this,   SLOT(at_resourcePool_resourceAdded(const QnResourcePtr &)));
    connect(resourcePool(), SIGNAL(resourceRemoved(const QnResourcePtr &)), this,   SLOT(at_resourcePool_resourceRemoved(const QnResourcePtr &)));
    foreach(const QnResourcePtr &resource, resourcePool()->getResources())
        at_resourcePool_resourceAdded(resource);

    connect(context(), &QnWorkbenchContext::userChanged, this, &QnWorkbenchUserEmailWatcher::forceCheckAll);
}

QnWorkbenchUserEmailWatcher::~QnWorkbenchUserEmailWatcher() {
    while(!m_emailValidByUser.empty())
        at_resourcePool_resourceRemoved(m_emailValidByUser.keys().back());
}

void QnWorkbenchUserEmailWatcher::forceCheck(const QnUserResourcePtr &user) {
    emit userEmailValidityChanged(user, m_emailValidByUser.value(user, QnEmailAddress::isValid(user->getEmail())));
}

void QnWorkbenchUserEmailWatcher::forceCheckAll() {
    QHash<QnUserResourcePtr, bool>::const_iterator iter = m_emailValidByUser.constBegin();
    while (iter != m_emailValidByUser.constEnd()) {
        emit userEmailValidityChanged(iter.key(), iter.value());
        ++iter;
    }
}

void QnWorkbenchUserEmailWatcher::at_resourcePool_resourceAdded(const QnResourcePtr &resource) {
    QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
    if(!user)
        return;

    connect(user, &QnUserResource::emailChanged, this, &QnWorkbenchUserEmailWatcher::at_user_emailChanged);

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

void QnWorkbenchUserEmailWatcher::at_user_emailChanged(const QnResourcePtr &resource) {
    QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
    if (!user)
        return;

    bool valid = QnEmailAddress::isValid(user->getEmail());
    if (m_emailValidByUser.contains(user) && m_emailValidByUser[user] == valid)
        return;

    m_emailValidByUser[user] = valid;
    emit userEmailValidityChanged(user, valid);
}
