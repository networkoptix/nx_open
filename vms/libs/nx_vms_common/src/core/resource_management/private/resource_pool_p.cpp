// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "resource_pool_p.h"

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource/user_resource.h>
#include <nx/utils/std/algorithm.h>
#include <nx/utils/log/log_main.h>

QnResourcePool::Private::Private(
    QnResourcePool* owner,
    nx::vms::common::SystemContext* systemContext)
    :
    q(owner),
    systemContext(systemContext)
{
}

void QnResourcePool::Private::handleResourceAdded(const QnResourcePtr& resource)
{
    if (const auto server = resource.dynamicCast<QnMediaServerResource>())
    {
        mediaServers.insert(server);
    }
    else if (const auto storage = resource.dynamicCast<QnStorageResource>())
    {
        storages.insert(storage);
    }
    else if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>())
    {
        resourcesByPhysicalId.insert(camera->getPhysicalId(), camera);
        QObject::connect(camera.data(), &QnVirtualCameraResource::isIOModuleChanged, q,
            [this, camera]()
            {
                NX_WRITE_LOCKER lk(&q->m_resourcesMutex);
                updateIsIOModule(camera);
            });

        updateIsIOModule(camera);
    }
    else if (const auto user = resource.dynamicCast<QnUserResource>())
    {
        const auto name = user->getName().toLower();
        usersByName[name].add(user);

        QObject::connect(user.data(), &QnUserResource::nameChanged, q,
            [this, user, name = std::make_unique<QString>(name)]()
            {
                NX_WRITE_LOCKER lk(&q->m_resourcesMutex);
                removeUser(*name, user);
                *name = user->getName().toLower();
                usersByName[*name].add(user);
            },
            Qt::DirectConnection);
        QObject::connect(user.data(), &QnUserResource::enabledChanged, q,
            [this, user]()
            {
                NX_WRITE_LOCKER lk(&q->m_resourcesMutex);
                usersByName[user->getName().toLower()].selectMainUser();
            },
            Qt::DirectConnection);
    }
}

void QnResourcePool::Private::handleResourceRemoved(const QnResourcePtr& resource)
{
    if (const auto server = resource.dynamicCast<QnMediaServerResource>())
    {
        mediaServers.remove(server);
    }
    else if (const auto storage = resource.dynamicCast<QnStorageResource>())
    {
        storages.remove(storage);
    }
    else if (const auto camera = resource.dynamicCast<QnVirtualCameraResource>())
    {
        resourcesByPhysicalId.remove(camera->getPhysicalId());
        camera->disconnect(q);
        ioModules.remove(camera);
        hasIoModules = !ioModules.isEmpty();
    }
    else if (const auto user = resource.dynamicCast<QnUserResource>())
    {
        user->disconnect(q);
        removeUser(user->getName().toLower(), user);
    }
}

void QnResourcePool::Private::updateIsIOModule(const QnVirtualCameraResourcePtr& camera)
{
    if (camera->isIOModule())
        ioModules.insert(camera);
    else
        ioModules.remove(camera);
    hasIoModules = !ioModules.isEmpty();
}

void QnResourcePool::Private::removeUser(const QString& name, const QnUserResourcePtr& user)
{
    const auto it = usersByName.find(name);
    if (!NX_ASSERT(it != usersByName.end(), "User name not found: %1", name))
        return;

    it->second.remove(user);
    NX_VERBOSE(
        this, "%1: Successfully removed user %2, userByName size: %3",
        __func__, name, it->second.count());

    if (it->second.empty())
        usersByName.erase(it);
}

static size_t userPriorityGroup(const QnUserResourcePtr& user)
{
    const auto t = user->userType();
    switch (t)
    {
        // Cloud user login is email, so it's unlikely to clash with other types. And it will newer
        // clash with built-in admin.
        case nx::vms::api::UserType::cloud: return 1;

        // Users managed by VMS administrators should always be prioritized over external users.
        case nx::vms::api::UserType::local:
        case nx::vms::api::UserType::temporaryLocal: return 2;

        // Users managed by LDAP should be fixed on LDAP by it's admin.
        case nx::vms::api::UserType::ldap: return 3;
     };

    NX_ASSERT(false, "Unexpected type %1 of %2", static_cast<int>(t), user);
    return 4;
}

void QnResourcePool::Private::SameNameUsers::add(QnUserResourcePtr user)
{
    NX_VERBOSE(this, "Add user: %1", user);
    m_byPriority[userPriorityGroup(user)].push_back(user);
    selectMainUser();
}

void QnResourcePool::Private::SameNameUsers::remove(const QnUserResourcePtr& user)
{
    auto priorityGroupIt = m_byPriority.find(userPriorityGroup(user));
    if (!NX_ASSERT(priorityGroupIt != m_byPriority.end(), "Unable to find group for %1", user))
        return;

    const auto result = nx::utils::erase_if(
        priorityGroupIt->second,
        [&](const auto& u) { return u == user; });
    if (!NX_ASSERT(result, "Unable to remove %1", user))
        return;

    if (priorityGroupIt->second.empty())
        m_byPriority.erase(priorityGroupIt);

    NX_VERBOSE(this, "Remove user: %1", user);
    selectMainUser();
}

int QnResourcePool::Private::SameNameUsers::count() const
{
    return m_byPriority.size();
}

void QnResourcePool::Private::SameNameUsers::selectMainUser()
{
    m_main.reset();
    m_hasClash = false;
    size_t disabled = 0;
    size_t total = 0;
    for (const auto& [p, users]: m_byPriority)
    {
        for (const auto& u: users)
        {
            if (!u->isEnabled())
            {
                ++disabled;
                continue;
            }

            if (m_main) //< We have a clash of the same priority!
            {
                NX_VERBOSE(this, "Clashed user name detected: %1", m_main->getName().toLower());
                m_main.reset();
                m_hasClash = true;
                return;
            }

            m_main = u;
        }
        total += users.size();

        if (m_main) //< Main of higher priority found.
        {
            NX_VERBOSE(this, "Selected main user: %1", m_main);
            return;
        }
    }

    if (disabled != 0 && disabled == total)
    {
        for (const auto& [p, users]: m_byPriority)
        {
            for (const auto& u: users)
            {
                m_main = users.front();
                NX_VERBOSE(this, "Selected disabled main user: %1", m_main);
                return;
            }
        }
    }

    NX_VERBOSE(this, "No user for name could be selected");
}
