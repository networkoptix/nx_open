// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "push_permission_manager.h"

#if !defined(Q_OS_IOS) && !defined(Q_OS_ANDROID)

namespace nx::vms::client::mobile::details {

struct PushPermissionManager::Private
{
    PushPermission permission = PushPermission::unknown;
};

PushPermissionManager::PushPermissionManager(QObject* parent):
    base_type(parent),
    d(new Private())
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

#endif
