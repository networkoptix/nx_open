// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QVector>
#include <vector>
#include <functional>

namespace nx::vms::client::desktop {
namespace entity_item_model {

class AbstractEntity;

class NX_VMS_CLIENT_DESKTOP_API IEntityNotification
{
public:
    virtual ~IEntityNotification() = default;

    virtual void dataChanged(int first, int last, const QVector<int>& roles) = 0;

    virtual void beginInsertRows(int first, int last) = 0;
    virtual void endInsertRows() = 0;

    virtual void beginRemoveRows(int first, int last) = 0;
    virtual void endRemoveRows() = 0;

    virtual void beginMoveRows(
        const AbstractEntity* source, int first, int last,
        const AbstractEntity* desination, int row) = 0;
    virtual void endMoveRows() = 0;

    virtual void layoutAboutToBeChanged(const std::vector<int>& indexPermutation) = 0;
    virtual void layoutChanged() = 0;
};

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
