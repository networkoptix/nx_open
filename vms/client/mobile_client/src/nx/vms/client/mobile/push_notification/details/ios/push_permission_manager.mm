// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "../push_permission_manager.h"

#include <UserNotifications/UserNotifications.h>

#include <QtCore/QTimer>
#include <QtCore/QPointer>

#include <utils/common/delayed.h>
#include <nx/utils/log/assert.h>

namespace {

using PushPermission = nx::vms::client::mobile::details::PushPermission;
using Callback = std::function<void (PushPermission /*value*/)>;

void getCurrentPermission(
    Callback callback)
{
    if (!callback)
        return;

    if ([UNUserNotificationCenter class] == nil)
        return;

    const auto notificationCenter = [UNUserNotificationCenter currentNotificationCenter];
    [notificationCenter getNotificationSettingsWithCompletionHandler:
        ^(UNNotificationSettings * _Nonnull settings)
        {
            const auto status = [settings authorizationStatus];
            switch (status)
            {
                case UNAuthorizationStatusNotDetermined:
                {
                    const UNAuthorizationOptions auth =
                        UNAuthorizationOptionAlert
                        | UNAuthorizationOptionSound
                        | UNAuthorizationOptionBadge;

                    [notificationCenter requestAuthorizationWithOptions: auth completionHandler:
                        ^(BOOL granted, NSError* _Nullable error)
                        {
                            if (error != nil)
                            {
                                callback(PushPermission::unknown);
                            }
                            else
                            {
                                callback(granted
                                    ? PushPermission::authorized
                                    : PushPermission::denied);
                            }
                        }];
                    break;
                }
                case UNAuthorizationStatusDenied:
                    callback(PushPermission::denied);
                    break;
                case UNAuthorizationStatusAuthorized:
                    callback(PushPermission::authorized);
                    break;
                case UNAuthorizationStatusProvisional:
                    NX_ASSERT(false, "Provisional push notification permission is not supported!");
                    break;
                default:
                    NX_ASSERT(false, "Unknown push notification permission.");
                    break;
            }
        }];
}
} // namespace

namespace nx::vms::client::mobile::details {

struct PushPermissionManager::Private
{
    PushPermissionManager* const q;

    QTimer updateTimer;
    PushPermission permission = PushPermission::unknown;
    bool updating = false;

    Private(PushPermissionManager* q);
};

PushPermissionManager::Private::Private(PushPermissionManager* q):
    q(q)
{
    const auto updatePermission =
        [this]()
        {
            if (updating)
                return;

            updating = true;
            getCurrentPermission(
                [this](PushPermission value)
                {
                    updating = false;
                    if (value == permission)
                        return;

                    permission = value;
                    emit this->q->permissionChanged();
                });
        };

    static constexpr int kUpdateIntervalMs = 5000;
    updateTimer.setInterval(kUpdateIntervalMs);
    connect(&updateTimer, &QTimer::timeout, q, updatePermission);
    updatePermission();
    updateTimer.start();
}

//--------------------------------------------------------------------------------------------------

PushPermissionManager::PushPermissionManager(QObject* parent):
    base_type(parent),
    d(new Private(this))
{
}

PushPermissionManager::~PushPermissionManager()
{
}

PushPermission PushPermissionManager::permission() const
{
    return d->permission;
}

} // namespace nx::vms::client::mobile::details
