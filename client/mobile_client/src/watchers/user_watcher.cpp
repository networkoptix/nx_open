#include "user_watcher.h"

#include <common/common_module.h>

#include <client/client_message_processor.h>

#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource_management/resource_pool.h>

#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>

QnUserWatcher::QnUserWatcher(QObject* parent) :
    base_type(parent)
{
    connect(QnClientMessageProcessor::instance(), &QnClientMessageProcessor::initialResourcesReceived,
        this, [this]
        {
            const auto users = qnResPool->getResources<QnUserResource>();
            const auto it = std::find_if(users.cbegin(), users.cend(),
                [this](const QnUserResourcePtr& user)
                {
                    return m_userName.compare(user->getName(), Qt::CaseInsensitive) == 0;
                }
            );
            setCurrentUser(it != users.cend() ? *it : QnUserResourcePtr());
        }
    );

    connect(qnResPool, &QnResourcePool::resourceRemoved,
        this, [this](const QnResourcePtr& resource)
        {
            const auto user = resource.dynamicCast<QnUserResource>();
            if (!user || user != m_user)
                return;

            setCurrentUser(QnUserResourcePtr());
        }
    );
}

const QnUserResourcePtr& QnUserWatcher::user() const
{
    return m_user;
}

void QnUserWatcher::setCurrentUser(const QnUserResourcePtr& user)
{
    if (m_user == user)
        return;

    m_user = user;

    emit userChanged(user);
}

const QString& QnUserWatcher::userName() const
{
    return m_userName;
}

void QnUserWatcher::setUserName(const QString& name)
{
    if (m_userName == name)
        return;

    m_userName = name;
    emit userNameChanged();
}
