// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_watcher.h"

#include <client/client_message_processor.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/utils/guarded_callback.h>
#include <nx/vms/client/core/system_context.h>
#include <utils/common/checked_cast.h>

namespace nx::vms::client::core {

namespace {

QnUserResourcePtr findUser(const QString& userName, const QnResourceList& resources)
{
    for (const auto& resource: resources)
    {
        if (auto user = resource.dynamicCast<QnUserResource>())
        {
            if (userName.compare(user->getName(), Qt::CaseInsensitive) == 0)
                return user;
        }
    }
    return {};
}

} // namespace

UserWatcher::UserWatcher(SystemContext* systemContext, QObject* parent):
    QObject(parent),
    SystemContextAware(systemContext)
{
    connect(resourcePool(), &QnResourcePool::resourcesAdded,
        this,
        [this](const QnResourceList& resources)
        {
            const auto username = QString::fromStdString(
                this->systemContext()->connectionCredentials().username);

            if (!m_user && !username.isEmpty())
            {
                if (const auto user = findUser(username, resources))
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
