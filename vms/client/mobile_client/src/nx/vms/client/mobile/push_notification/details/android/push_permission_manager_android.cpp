// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "../push_permission_manager.h"

#include <QtCore/QTimer>
#include <QtCore/QJniObject>

namespace nx::vms::client::mobile::details {

namespace {

static constexpr auto kPushMessageManagerClass =
    "com/nxvms/mobile/push/PushMessageManager";

} // namespace

struct PushPermissionManager::Private: public QObject
{
    PushPermissionManager* const q;
    PushPermission permission = PushPermission::unknown;
    QTimer updateTimer;

    Private(PushPermissionManager* q);
    void updatePermission();
    void requestPermissionIfNeeded();
};

PushPermissionManager::Private::Private(PushPermissionManager* q):
    q(q)
{
    requestPermissionIfNeeded();

    static constexpr int kUpdateIntervalMs = 3000;
    connect(&updateTimer, &QTimer::timeout, this, &Private::updatePermission);
    updateTimer.setInterval(kUpdateIntervalMs);
    updatePermission();
    updateTimer.start();

    updatePermission();
}

void PushPermissionManager::Private::updatePermission()
{
    const bool enabled = QJniObject::callStaticMethod<jboolean>(
        kPushMessageManagerClass, "notificationsEnabled", "()Z");

    const bool hasChanges = permission == PushPermission::unknown
        || (enabled != (permission == PushPermission::authorized));
    if (!hasChanges)
        return;

    permission = enabled
        ? PushPermission::authorized
        : PushPermission::denied;

    q->permissionChanged();
}

void PushPermissionManager::Private::requestPermissionIfNeeded()
{
    QJniObject::callStaticMethod<void>(
        kPushMessageManagerClass, "requestPushNotificationPermission", "()V");
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
