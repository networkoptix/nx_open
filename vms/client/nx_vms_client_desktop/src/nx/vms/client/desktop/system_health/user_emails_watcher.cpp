// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_emails_watcher.h"

#include <core/resource/user_resource.h>
#include <nx/utils/qt_helpers.h>
#include <nx/vms/client/core/resource/session_resources_signal_listener.h>
#include <nx/vms/client/desktop/system_context.h>
#include <utils/email/email.h>

namespace nx::vms::client::desktop {

namespace {

bool userHasInvalidEmail(const QnUserResourcePtr& user)
{
    return NX_ASSERT(user)
        && user->resourcePool()
        && user->isEnabled()
        && !nx::email::isValidAddress(user->getEmail());
}

} // namespace

UserEmailsWatcher::UserEmailsWatcher(SystemContext* systemContext, QObject* parent):
    QObject(parent),
    SystemContextAware(systemContext)
{
    auto userChangesListener = new core::SessionResourcesSignalListener<QnUserResource>(
        systemContext,
        this);

    const auto checkUser =
        [this](const QnUserResourcePtr& user)
        {
            if (userHasInvalidEmail(user))
                setUsersWithInvalidEmail(m_usersWithInvalidEmail + QnUserResourceSet{user});
            else
                setUsersWithInvalidEmail(m_usersWithInvalidEmail - QnUserResourceSet{user});
        };
    userChangesListener->addOnSignalHandler(&QnUserResource::emailChanged, checkUser);
    userChangesListener->addOnSignalHandler(&QnUserResource::enabledChanged, checkUser);

    userChangesListener->setOnAddedHandler(
        [this](const QnUserResourceList& users)
        {
            QnUserResourceSet usersWithInvalidEmail;
            for (const auto& user: std::as_const(users))
            {
                if (userHasInvalidEmail(user))
                    usersWithInvalidEmail.insert(user);
            }
            setUsersWithInvalidEmail(m_usersWithInvalidEmail + usersWithInvalidEmail);
        });

    userChangesListener->setOnRemovedHandler(
        [this](const QnUserResourceList& users)
        {
            setUsersWithInvalidEmail(m_usersWithInvalidEmail - nx::utils::toQSet(users));
        });

    userChangesListener->start();
}

UserEmailsWatcher::~UserEmailsWatcher()
{
}

QnUserResourceSet UserEmailsWatcher::usersWithInvalidEmail() const
{
    return m_usersWithInvalidEmail;
}

void UserEmailsWatcher::setUsersWithInvalidEmail(QnUserResourceSet value)
{
    if (value == m_usersWithInvalidEmail)
        return;

    m_usersWithInvalidEmail = value;
    emit userSetChanged();
}

} // namespace nx::vms::client::desktop
