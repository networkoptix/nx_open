// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "entity_notification_guard.h"

namespace nx::vms::client::desktop {
namespace entity_item_model {

void dataChanged(IEntityNotification* entityNotification,
    const QVector<int>& roles, int first, int count)
{
    if (entityNotification && count > 0)
        entityNotification->dataChanged(first, first + count - 1, roles);
}

NotificationGuard::NotificationGuard(const Finalizer& finalizer):
    m_finalizer(finalizer)
{
}

NotificationGuard::~NotificationGuard()
{
    if (m_finalizer)
        m_finalizer();
}

NotificationGuard insertRowsGuard(IEntityNotification* entityNotification, int first, int count)
{
    if (!entityNotification || count == 0)
        return NotificationGuard({});

    entityNotification->beginInsertRows(first, first + count - 1);

    return NotificationGuard([entityNotification] { entityNotification->endInsertRows(); });
}

NotificationGuard removeRowsGuard(IEntityNotification* entityNotification, int first, int count)
{
    if (!entityNotification || count == 0)
        return NotificationGuard({});

    entityNotification->beginRemoveRows(first, first + count - 1);

    return NotificationGuard([entityNotification] { entityNotification->endRemoveRows(); });
}

NotificationGuard moveRowsGuard(IEntityNotification* entityNotification,
    const AbstractEntity* source, int first, int count,
    const AbstractEntity* desination, int beforeRow)
{
    if (!entityNotification || count == 0)
        return NotificationGuard({});

    entityNotification->beginMoveRows(source, first, first + count - 1, desination, beforeRow);

    return NotificationGuard([entityNotification] { entityNotification->endMoveRows(); });
}

NotificationGuard layoutChangeGuard(IEntityNotification* entityNotification,
    const std::vector<int>& indexPermutation)
{
    if (!entityNotification)
        return NotificationGuard({});

    entityNotification->layoutAboutToBeChanged(indexPermutation);

    return NotificationGuard([entityNotification] { entityNotification->layoutChanged(); });
}

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
