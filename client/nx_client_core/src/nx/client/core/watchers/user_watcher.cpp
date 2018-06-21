#include "user_watcher.h"

#include <common/common_module.h>

#include <client/client_message_processor.h>

#include <core/resource/resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>

namespace {

QnUserResourcePtr findUser(const QString& userName, QnCommonModule* commonModule)
{
    NX_ASSERT(commonModule);
    if (!commonModule)
        return QnUserResourcePtr();

    const auto users = commonModule->resourcePool()->getResources<QnUserResource>();

    /* TODO: Remove this hack which makes QnAvailableCamerasWatcher working.
       Now it always needs a user resource, but when we connect from Lite Client we use
       fake user name, so we don't have such user in the resource pool.
       This hack picks up the system owner when we connect with such user name.
       This is enough to make the cameras watcher working but not semantically correct.
   */
    const auto serverId = QnUuid::fromStringSafe(userName);
    if (!serverId.isNull() && commonModule->remoteGUID() == serverId)
        return commonModule->resourcePool()->getAdministrator();

    for (const auto& user: users)
    {
        if (userName.compare(user->getName(), Qt::CaseInsensitive) == 0)
            return user;
    }

    return QnUserResourcePtr();
}

} // namespace

namespace nx {
namespace client {
namespace core {

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
    connect(this, &UserWatcher::userChanged, this,
        [this]() { setUserName(user()->getName()); });
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

} // namespace core
} // namespace client
} // namespace nx
