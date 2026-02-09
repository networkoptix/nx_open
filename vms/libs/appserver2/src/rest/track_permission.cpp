// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "track_permission.h"

#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource/storage_resource.h>
#include <core/resource/user_resource.h>
#include <core/resource/videowall_resource.h>
#include <core/resource/webpage_resource.h>
#include <core/resource_access/resource_access_manager.h>
#include <core/resource_access/resource_access_subject.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/common/system_context.h>

struct TrackPermission::Private
{
    using Notifier = nx::core::access::AccessRightsResolver::Notifier;
    using Notification = nx::MoveOnlyFunc<void(bool noEtag)>;
    using NotificationList = std::vector<Notification>;
    using NotifyType = nx::network::rest::Handler::NotifyType;

    struct Accessible
    {
        bool all = false;
        std::set<QString> ids;
    };

    struct Subscription
    {
        std::unique_ptr<Notifier> notifier;
        std::vector<nx::utils::SharedGuardPtr> connectionGuards;
        Accessible accessible;
        int count = 0;
    };

    TrackPermission* q;
    nx::vms::common::SystemContext* systemContext;
    nx::Uuid allAccessibleGroupId;
    nx::vms::api::AccessRight requiredAccess;
    nx::MoveOnlyFunc<void(const QString& id,
        nx::network::rest::Handler::NotifyType notifyType,
        std::optional<nx::Uuid> user,
        bool noEtag)>
        notifyFunc;
    nx::Lockable<std::map<nx::Uuid, Subscription>> userSubscriptions;

    Private(
        TrackPermission* q,
        nx::vms::common::SystemContext* systemContext,
        nx::Uuid allAccessibleGroupId,
        nx::vms::api::AccessRight requiredAccess,
        nx::MoveOnlyFunc<void(const QString& id,
            nx::network::rest::Handler::NotifyType notifyType,
            std::optional<nx::Uuid> user,
            bool noEtag)> notify)
        :
        q(q),
        systemContext(systemContext),
        allAccessibleGroupId(std::move(allAccessibleGroupId)),
        requiredAccess(requiredAccess),
        notifyFunc(std::move(notify))
    {
    }

    static void notify(NotificationList notifications)
    {
        if (notifications.empty())
            return;

        auto last = std::prev(notifications.end());
        for (auto it = notifications.begin(); it != notifications.end(); ++it)
            (*it)(/*noEtag*/ it != last);
    }

    void at_accessChanged(const nx::Uuid& user)
    {
        NotificationList notifications;
        {
            auto lock = userSubscriptions.lock();
            if (auto it = lock->find(user); it != lock->end())
                changeAccessible(user, &it->second.accessible, &notifications);
        }
        notify(std::move(notifications));
    }

    void at_accessReset()
    {
        NotificationList notifications;
        {
            auto lock = userSubscriptions.lock();
            for (auto& [user, subscription]: *lock)
                changeAccessible(user, &subscription.accessible, &notifications);
        }
        notify(std::move(notifications));
    }

    nx::utils::Guard trackPermissions(nx::Uuid user)
    {
        auto lock = userSubscriptions.lock();
        auto it = lock->find(user);
        if (it == lock->end())
        {
            auto manager = systemContext->resourceAccessManager();
            Subscription subscription;
            const bool all = areAllAccessible(user);
            subscription.accessible = {all, q->accessibleIds(user, all)};
            subscription.notifier.reset(manager->createNotifier());
            subscription.notifier->setSubjectId(user);
            subscription.connectionGuards = {
                nx::qtDirectConnect(subscription.notifier.get(), &Notifier::resourceAccessChanged,
                    [this](const auto& user, auto) { at_accessChanged(user); }),
                nx::qtDirectConnect(manager, &QnResourceAccessManager::resourceAccessReset,
                    [this] { at_accessReset(); })
            };
            it = lock->emplace(user, std::move(subscription)).first;
        }

        ++it->second.count;
        return {
            [this, user = std::move(user)]
            {
                auto lock = userSubscriptions.lock();
                auto it = lock->find(user);
                if ((it != lock->end()) && (--it->second.count == 0))
                    lock->erase(it);
            }};
    }

    void addAccessible(const QString& id)
    {
        auto lock = userSubscriptions.lock();
        for (auto& [_, subscription]: *lock)
            subscription.accessible.ids.insert(id);
    }

    void removeAccessible(const QString& id)
    {
        auto lock = userSubscriptions.lock();
        for (auto& [_, subscription]: *lock)
            subscription.accessible.ids.erase(id);
    }

    Notification makeNotification(nx::Uuid user, QString id, NotifyType type) const
    {
        return
            [this, user = std::move(user), id = std::move(id), type](bool noEtag) mutable
            {
                notifyFunc(id, type, std::move(user), noEtag);
            };
    }

    void changeAccessible(
        const nx::Uuid& user, Accessible* accessibleInOut, NotificationList* notificationsInOut)
    {
        const bool all = areAllAccessible(user);
        if (all && accessibleInOut->all)
            return;

        auto ids = q->accessibleIds(user, all);
        auto newIt = ids.begin();
        auto oldIt = accessibleInOut->ids.begin();
        while (newIt != ids.end() && oldIt != accessibleInOut->ids.end())
        {
            const int compare = oldIt->compare(*newIt);
            if (compare < 0)
            {
                notificationsInOut->push_back(makeNotification(user, *oldIt, NotifyType::delete_));
                ++oldIt;
            }
            else if (compare == 0)
            {
                ++oldIt;
                ++newIt;
            }
            else
            {
                notificationsInOut->push_back(makeNotification(user, *newIt, NotifyType::update));
            }
        }
        for (; oldIt != accessibleInOut->ids.end(); ++oldIt)
            notificationsInOut->push_back(makeNotification(user, *oldIt, NotifyType::delete_));
        for (; newIt != ids.end(); ++newIt)
            notificationsInOut->push_back(makeNotification(user, *newIt, NotifyType::update));
        accessibleInOut->all = all;
        accessibleInOut->ids = std::move(ids);
    }

    bool areAllAccessible(const nx::Uuid& user) const
    {
        if (user == QnUserResource::kAdminGuid
            || user == nx::network::rest::kCloudServiceUserAccess.userId
            || user == nx::network::rest::kVideowallUserAccess.userId)
        {
            return true;
        }

        auto userResource = systemContext->resourcePool()->getResourceById<QnUserResource>(user);
        if (!userResource || !userResource->isEnabled())
            return false;

        if (userResource->isAdministrator())
            return true;

        for (const auto& g: userResource->allGroupIds())
        {
            if (nx::vms::api::kPredefinedGroupIds.contains(g))
                return true;
        }

        return !allAccessibleGroupId.isNull()
            && systemContext->resourceAccessManager()->hasAccessRights(
                userResource, allAccessibleGroupId, requiredAccess);
    }

    template<typename T>
    std::set<QString> accessibleIds(const nx::Uuid& user, bool all) const
    {
        std::set<QString> result;
        if (all)
        {
            systemContext->resourcePool()->forEach<T>(
                [&result](auto r) { result.insert(r->getId().toSimpleString()); });
            return result;
        }

        auto userResource = systemContext->resourcePool()->getResourceById<QnUserResource>(user);
        if (!userResource)
            return result;

        systemContext->resourcePool()->forEach<T>(
            [&result, userResource, m = systemContext->resourceAccessManager(), a = requiredAccess](
                auto r)
            {
                if (m->hasAccessRights(userResource, r, a))
                    result.insert(r->getId().toSimpleString());
            });
        return result;
    }
};

TrackPermission::TrackPermission(
    nx::vms::common::SystemContext* systemContext,
    nx::Uuid allAccessibleGroupId,
    nx::vms::api::AccessRight requiredAccess,
    nx::MoveOnlyFunc<void(const QString& id,
        nx::network::rest::Handler::NotifyType notifyType,
        std::optional<nx::Uuid> user,
        bool noEtag)> notify)
    :
    d(std::make_unique<Private>(
        this, systemContext, std::move(allAccessibleGroupId), requiredAccess, std::move(notify)))
{
}

TrackPermission::~TrackPermission() = default;

nx::utils::Guard TrackPermission::trackPermissions(const nx::Uuid& user)
{
    return d->trackPermissions(user);
}

void TrackPermission::addAccessible(const QString& id)
{
    d->addAccessible(id);
}

void TrackPermission::removeAccessible(const QString& id)
{
    d->removeAccessible(id);
}

template<typename T>
std::set<QString> TrackPermission::accessibleIds(const nx::Uuid& user, bool all) const
{
    return d->accessibleIds<T>(user, all);
}

template std::set<QString> TrackPermission::accessibleIds<QnLayoutResource>(
    const nx::Uuid&, bool) const;

template std::set<QString> TrackPermission::accessibleIds<QnVirtualCameraResource>(
    const nx::Uuid&, bool) const;

template std::set<QString> TrackPermission::accessibleIds<QnMediaServerResource>(
    const nx::Uuid&, bool) const;

template std::set<QString> TrackPermission::accessibleIds<QnStorageResource>(
    const nx::Uuid&, bool) const;

template std::set<QString> TrackPermission::accessibleIds<QnVideoWallResource>(
    const nx::Uuid&, bool) const;

template std::set<QString> TrackPermission::accessibleIds<QnWebPageResource>(
    const nx::Uuid&, bool) const;
