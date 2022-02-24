// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QVector>
#include <functional>

#include "entity_notification_interface.h"

namespace nx::vms::client::desktop {
namespace entity_item_model {

class AbstractEntity;

/**
 * A scoped guard alike class which helps to do consistent begin*** end*** notifications.
 */
class NX_VMS_CLIENT_DESKTOP_API NotificationGuard
{
public:
    using Finalizer = std::function<void()>;
    NotificationGuard(const Finalizer& finalizer);
    NotificationGuard(const NotificationGuard&) = delete;
    NotificationGuard& operator=(const NotificationGuard&) = delete;
    ~NotificationGuard();

private:
    Finalizer m_finalizer;
};

NX_VMS_CLIENT_DESKTOP_API void dataChanged(
    IEntityNotification* entityNotification, const QVector<int>& roles, int first, int count = 1);

NX_VMS_CLIENT_DESKTOP_API NotificationGuard insertRowsGuard(
    IEntityNotification* entityNotification, int first, int count = 1);

NX_VMS_CLIENT_DESKTOP_API NotificationGuard removeRowsGuard(
    IEntityNotification* entityNotification, int first, int count = 1);

NX_VMS_CLIENT_DESKTOP_API NotificationGuard moveRowsGuard(IEntityNotification* entityNotification,
    const AbstractEntity* source, int first, int count,
    const AbstractEntity* desination, int beforeRow);

NX_VMS_CLIENT_DESKTOP_API NotificationGuard layoutChangeGuard(
    IEntityNotification* entityNotification, const std::vector<int>& indexPermutation);

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
