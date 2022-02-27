// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "user_watcher.h"

#include <client/client_message_processor.h>
#include <common/common_module.h>
#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <utils/common/checked_cast.h>

#include <nx/utils/guarded_callback.h>

namespace {

QnUserResourcePtr findUser(const QString& userName, QnCommonModule* commonModule)
{
    NX_ASSERT(commonModule);
    if (!commonModule)
        return QnUserResourcePtr();

    const auto users = commonModule->resourcePool()->getResources<QnUserResource>();

    for (const auto& user: users)
    {
        if (userName.compare(user->getName(), Qt::CaseInsensitive) == 0)
            return user;
    }

    return QnUserResourcePtr();
}

} // namespace

namespace nx::vms::client::core {

UserWatcher::UserWatcher(QObject* parent) :
    base_type(parent)
{
    const auto updateUserResource =
        [this] { setUser(findUser(m_userName, commonModule())); };

    connect(qnClientMessageProcessor, &QnClientMessageProcessor::initialResourcesReceived,
        this, updateUserResource);

    connect(resourcePool(), &QnResourcePool::resourceRemoved,
        this, [this](const QnResourcePtr& resource)
        {
            const auto user = resource.dynamicCast<QnUserResource>();
            if (user && user == m_user)
                setUser(QnUserResourcePtr());
        }
    );

    connect(this, &UserWatcher::userNameChanged, this, updateUserResource);

    const auto updateUserName =
        [this](const QnUserResourcePtr& user) { setUserName(user ? user->getName() : QString()); };
    connect(this, &UserWatcher::userChanged, this, nx::utils::guarded(this, updateUserName));
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
}

const QString& UserWatcher::userName() const
{
    return m_userName;
}

void UserWatcher::setUserName(const QString& name)
{
    if (m_userName == name)
        return;

    m_userName = name;
    emit userNameChanged();
}

} // namespace nx::vms::client::core
