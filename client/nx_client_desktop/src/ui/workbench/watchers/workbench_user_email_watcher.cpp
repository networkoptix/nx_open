#include "workbench_user_email_watcher.h"

#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>

#include <utils/email/email.h>

#include <ui/workbench/workbench_context.h>

QnWorkbenchUserEmailWatcher::QnWorkbenchUserEmailWatcher(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent)
{
    connect(resourcePool(), &QnResourcePool::resourceAdded, this,
        &QnWorkbenchUserEmailWatcher::at_resourcePool_resourceAdded);
    connect(resourcePool(), &QnResourcePool::resourceRemoved, this,
        &QnWorkbenchUserEmailWatcher::at_resourcePool_resourceRemoved);

    for (const auto& user : resourcePool()->getResources<QnUserResource>())
        at_resourcePool_resourceAdded(user);

    connect(context(), &QnWorkbenchContext::userChanged, this,
        &QnWorkbenchUserEmailWatcher::forceCheckAll);
}

QnWorkbenchUserEmailWatcher::~QnWorkbenchUserEmailWatcher()
{
    while (!m_emailValidByUser.empty())
        at_resourcePool_resourceRemoved(m_emailValidByUser.keys().back());
}

void QnWorkbenchUserEmailWatcher::forceCheck(const QnUserResourcePtr &user)
{
    if (!context()->user())
        return;

    emit userEmailValidityChanged(user, m_emailValidByUser.value(user, isUserEmailValid(user)));
}

void QnWorkbenchUserEmailWatcher::forceCheckAll()
{
    if (!context()->user())
        return;

    QHash<QnUserResourcePtr, bool>::const_iterator iter = m_emailValidByUser.constBegin();
    while (iter != m_emailValidByUser.constEnd())
    {
        emit userEmailValidityChanged(iter.key(), iter.value());
        ++iter;
    }
}

bool QnWorkbenchUserEmailWatcher::isUserEmailValid(const QnUserResourcePtr &user) const
{
    NX_ASSERT(user, Q_FUNC_INFO, "User must exist here");
    if (!user)
        return true;

    /* Suppress errors for disabled users. */
    return !user->isEnabled() || nx::email::isValidAddress(user->getEmail());
}


void QnWorkbenchUserEmailWatcher::at_resourcePool_resourceAdded(const QnResourcePtr &resource)
{
    QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
    if (!user)
        return;

    auto safeCheck = [this](const QnResourcePtr &resource)
        {
            QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
            if (!user)
                return;
            checkUser(user);
        };

    // TODO: #GDM change signal signature
    connect(user, &QnUserResource::emailChanged, this, safeCheck);
    connect(user, &QnUserResource::enabledChanged, this, &QnWorkbenchUserEmailWatcher::checkUser);
    checkUser(user);
}

void QnWorkbenchUserEmailWatcher::at_resourcePool_resourceRemoved(const QnResourcePtr &resource)
{
    QnUserResourcePtr user = resource.dynamicCast<QnUserResource>();
    if (!user)
        return;

    disconnect(user.data(), NULL, this, NULL);

    /* Any email is valid for removed user. */
    if (!m_emailValidByUser[user])
        emit userEmailValidityChanged(user, true);
    m_emailValidByUser.remove(user);
}

void QnWorkbenchUserEmailWatcher::checkUser(const QnUserResourcePtr &user)
{
    bool valid = isUserEmailValid(user);
    if (m_emailValidByUser.contains(user) && m_emailValidByUser[user] == valid)
        return;

    m_emailValidByUser[user] = valid;

    if (!context()->user())
        return;
    emit userEmailValidityChanged(user, valid);
}
