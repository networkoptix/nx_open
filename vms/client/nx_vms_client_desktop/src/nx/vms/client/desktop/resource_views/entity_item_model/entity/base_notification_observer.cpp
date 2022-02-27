// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "base_notification_observer.h"

namespace nx::vms::client::desktop {
namespace entity_item_model {

void BaseNotificatonObserver::dataChanged(int, int, const QVector<int>&)
{
}

void BaseNotificatonObserver::beginInsertRows(int, int)
{
}

void BaseNotificatonObserver::endInsertRows()
{
}

void BaseNotificatonObserver::beginRemoveRows(int, int)
{
}

void BaseNotificatonObserver::endRemoveRows()
{
}

void BaseNotificatonObserver::beginMoveRows(const AbstractEntity*, int, int,
    const AbstractEntity*, int)
{
}

void BaseNotificatonObserver::endMoveRows()
{
}

void BaseNotificatonObserver::layoutAboutToBeChanged(const std::vector<int>&)
{
}

void BaseNotificatonObserver::layoutChanged()
{
}

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
