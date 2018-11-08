#include "user_emails_watcher.h"

#include <api/common_message_processor.h>
#include <common/common_module.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_changes_listener.h>
#include <core/resource_management/resource_pool.h>

#include <utils/email/email.h>

#include <ui/workbench/workbench_context.h>

namespace nx::vms::client::desktop {

UserEmailsWatcher::UserEmailsWatcher(QObject* parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
    const auto messageProcessor = commonModule()->messageProcessor();
    NX_ASSERT(messageProcessor);

    connect(messageProcessor, &QnCommonMessageProcessor::initialResourcesReceived, this,
        [this]()
        {
            m_processRuntimeUpdates = true;
            checkAll();
        });

    connect(messageProcessor, &QnCommonMessageProcessor::connectionClosed, this,
        [this]()
        {
            const bool changed = !m_usersWithInvalidEmail.empty();
            m_usersWithInvalidEmail = QnUserResourceSet();
            m_processRuntimeUpdates = false;
            if (changed)
                emit userSetChanged();
        });

    const auto runtimeCheckUser =
        [this](const QnUserResourcePtr& user)
        {
            if (m_processRuntimeUpdates)
                checkUser(user);
        };

    auto userChangesListener = new QnResourceChangesListener(this);
    userChangesListener->connectToResources<QnUserResource>(
        &QnUserResource::emailChanged, runtimeCheckUser);

    userChangesListener->connectToResources<QnUserResource>(
        &QnUserResource::enabledChanged, runtimeCheckUser);

    checkAll();

    m_processRuntimeUpdates = !commonModule()->remoteGUID().isNull();
}

UserEmailsWatcher::~UserEmailsWatcher()
{
}

QnUserResourceSet UserEmailsWatcher::usersWithInvalidEmail() const
{
    return m_usersWithInvalidEmail;
}

bool UserEmailsWatcher::userHasInvalidEmail(const QnUserResourcePtr& user)
{
    return user && user->resourcePool() && user->isEnabled()
        && !nx::email::isValidAddress(user->getEmail());
}

void UserEmailsWatcher::checkUser(const QnUserResourcePtr& user)
{
    if (!user)
        return;

    if (userHasInvalidEmail(user))
    {
        if (!m_usersWithInvalidEmail.contains(user))
        {
            m_usersWithInvalidEmail.insert(user);
            emit userSetChanged();
        }
    }
    else
    {
        if (m_usersWithInvalidEmail.remove(user))
            emit userSetChanged();
    }
}

void UserEmailsWatcher::checkAll()
{
    QnUserResourceSet previousSet = m_usersWithInvalidEmail;
    m_usersWithInvalidEmail = QnUserResourceSet();

    for (const auto& user: resourcePool()->getResources<QnUserResource>())
    {
        if (userHasInvalidEmail(user))
            m_usersWithInvalidEmail.insert(user);
    }

    if (previousSet != m_usersWithInvalidEmail)
        emit userSetChanged();
}

} // namespace nx::vms::client::desktop
