// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_watcher.h"

#include <core/resource/media_server_resource.h>
#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/core/system_context.h>
#include <utils/common/checked_cast.h>

namespace nx::vms::client::core {

UserWatcher::UserWatcher(SystemContext* systemContext, QObject* parent):
    QObject(parent),
    SystemContextAware(systemContext)
{
    connect(resourcePool(), &QnResourcePool::resourcesAdded,
        this,
        [this](const QnResourceList&)
        {
            const auto username = QString::fromStdString(
                this->systemContext()->connectionCredentials().username);

            if (!m_user && !username.isEmpty())
            {
                // Here we use the same method as server in order to select the correct user when
                // there are multiple users with the same name.
                if (const auto user = resourcePool()->userByName(username).first)
                    setUser(user);
            }
        }
    );

    connect(resourcePool(), &QnResourcePool::resourcesRemoved,
        this,
        [this](const QnResourceList& resources)
        {
            if (m_user && resources.contains(m_user))
                setUser(QnUserResourcePtr());
        }
    );
}

const QnUserResourcePtr& UserWatcher::user() const
{
    return m_user;
}

void UserWatcher::setUser(const QnUserResourcePtr& user)
{
    if (m_user == user)
        return;

    m_user = user;

    emit userChanged(user);
    emit userNameChanged();
}

QString UserWatcher::userName() const
{
    return m_user ? m_user->getName() : QString();
}

} // namespace nx::vms::client::core
