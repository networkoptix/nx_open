// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "single_item_entity.h"

#include <nx/utils/log/assert.h>

#include <nx/vms/client/desktop/resource_views/entity_item_model/entity_model_mapping.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/entity_notification_guard.h>

namespace nx::vms::client::desktop {
namespace entity_item_model {

SingleItemEntity::SingleItemEntity(AbstractItemPtr item):
    base_type(),
    m_item(std::move(item))
{
    if (!m_item)
        return;

    m_item->setDataChangedCallback(
        [this](const QVector<int>& roles) { dataChanged(modelMapping(), roles, 0); });
}

SingleItemEntity::~SingleItemEntity()
{
    if (m_item)
        auto guard = removeRowsGuard(modelMapping(), 0);
}

int SingleItemEntity::rowCount() const
{
    return 1;
}

AbstractEntity* SingleItemEntity::childEntity(int row) const
{
    return nullptr;
}

int SingleItemEntity::childEntityRow(const AbstractEntity* entity) const
{
    return -1;
}

QVariant SingleItemEntity::data(int row, int role) const
{
    if (!NX_ASSERT(row == 0) || !m_item)
        return QVariant();

    return m_item->data(role);
}

Qt::ItemFlags SingleItemEntity::flags(int row) const
{
    if (!NX_ASSERT(row == 0) || !m_item)
        return Qt::ItemFlags();

    return m_item->flags();
}

bool SingleItemEntity::isPlanar() const
{
    return true;
}

SingleItemEntityPtr makeSingleItemEntity(AbstractItemPtr item)
{
    return std::make_unique<SingleItemEntity>(std::move(item));
}

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
