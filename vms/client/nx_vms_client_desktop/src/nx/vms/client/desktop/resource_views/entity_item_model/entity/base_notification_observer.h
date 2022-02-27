// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/entity_notification_interface.h>

namespace nx::vms::client::desktop {
namespace entity_item_model {

/**
 * Dummy implementation of IEntityNotification interface. It's no more pure virtual and
 * may be used for convenience sake: if you need observe only specific kind of notification then
 * you shouldn't make empty implementation of all irrelevant methods.
 */
class NX_VMS_CLIENT_DESKTOP_API BaseNotificatonObserver: public IEntityNotification
{
public:
    virtual void dataChanged(int first, int last, const QVector<int>& roles) override;

    virtual void beginInsertRows(int first, int last) override;
    virtual void endInsertRows() override;

    virtual void beginRemoveRows(int first, int last) override;
    virtual void endRemoveRows() override;

    virtual void beginMoveRows(
        const AbstractEntity* source, int first, int last,
        const AbstractEntity* desination, int row) override;
    virtual void endMoveRows() override;

    virtual void layoutAboutToBeChanged(const std::vector<int>& indexPermutation) override;
    virtual void layoutChanged() override;
};

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
