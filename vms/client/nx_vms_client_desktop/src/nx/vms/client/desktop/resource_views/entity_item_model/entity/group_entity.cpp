// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "group_entity.h"

#include <client/client_globals.h>

#include <nx/utils/log/assert.h>

#include <nx/vms/client/desktop/resource_views/entity_item_model/entity_model_mapping.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/entity_notification_guard.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/item/generic_item/generic_item_builder.h>
#include <nx/vms/client/desktop/resource_views/data/resource_extra_status.h>

namespace {

using ResourceExtraStatus = nx::vms::client::desktop::ResourceExtraStatus;
using AbstractEntity = nx::vms::client::desktop::entity_item_model::AbstractEntity;

QVariant groupedDeviceExtraStatus(const AbstractEntity* childEntity)
{
    if (!childEntity)
        return QVariant();

    ResourceExtraStatus result;
    for (int row = 0; row < childEntity->rowCount(); ++row)
    {
        const auto extraStatusData = childEntity->data(row, Qn::ResourceExtraStatusRole);
        if (!extraStatusData.isNull())
            result |= extraStatusData.value<ResourceExtraStatus>();
    }
    return QVariant::fromValue(result);
}

} // namespace

namespace nx::vms::client::desktop {
namespace entity_item_model {

GroupEntity::GroupEntity(AbstractItemPtr headItem):
    base_type(),
    m_headItem(std::move(headItem))
{
    NX_ASSERT(m_headItem, "Group head item should be defined");
    if (!m_headItem)
        m_headItem = GenericItemBuilder();
    setupHeadItemNotifications();
}

GroupEntity::GroupEntity(AbstractItemPtr headItem, AbstractEntityPtr nestedEntity):
    base_type(),
    m_headItem(std::move(headItem)),
    m_nestedEntity(std::move(nestedEntity))
{
    NX_ASSERT(m_headItem, "Group head item should be defined");
    if (!m_headItem)
        m_headItem = GenericItemBuilder();
    setupHeadItemNotifications();

    if (m_nestedEntity)
        assignAsChildEntity(m_nestedEntity.get());
}

GroupEntity::GroupEntity(
    AbstractItemPtr headItem,
    const EntityCreator& nestedEntityCreator,
    const QVector<int>& discardingRoles)
    :
    m_headItem(std::move(headItem)),
    m_nestedEntity(nestedEntityCreator()),
    m_discardingRoles(discardingRoles),
    m_nestedEntityCreator(nestedEntityCreator)
{
    NX_ASSERT(m_headItem, "Group head item should be defined");
    if (!m_headItem)
        m_headItem = GenericItemBuilder();
    setupHeadItemNotifications();

    if (m_nestedEntity)
        assignAsChildEntity(m_nestedEntity.get());
}

GroupEntity::~GroupEntity()
{
    auto guard = removeRowsGuard(modelMapping(), 0, rowCount());
}

void GroupEntity::setNestedEntity(AbstractEntityPtr entity)
{
    if (m_nestedEntity == entity)
        return;

    if (m_nestedEntity)
    {
        m_nestedEntity->hide();
        unassignEntity(m_nestedEntity.get());
        m_nestedEntity.reset();
    }

    if (entity)
    {
        entity->hide();
        m_nestedEntity = std::move(entity);
        assignAsChildEntity(m_nestedEntity.get());
        m_nestedEntity->show();
    }
}

int GroupEntity::rowCount() const
{
    return 1;
}

AbstractEntity* GroupEntity::childEntity(int row) const
{
    return (row == 0) ? m_nestedEntity.get() : nullptr;
}

int GroupEntity::childEntityRow(const AbstractEntity* entity) const
{
    if (m_nestedEntity && entity)
        return 0;

    return -1;
}

QVariant GroupEntity::data(int row, int role) const
{
    // TODO: #vbreus Should be provided as delegate by head item.
    using NodeType = ResourceTree::NodeType;
    if (row == 0
        && role == Qn::ResourceExtraStatusRole
        && m_headItem->data(Qn::NodeTypeRole).value<NodeType>() == NodeType::recorder)
    {
        return groupedDeviceExtraStatus(m_nestedEntity.get());
    }

    return (row == 0) ? m_headItem->data(role) : QVariant();
}

Qt::ItemFlags GroupEntity::flags(int row) const
{
    return (row == 0) ? m_headItem->flags() : Qt::ItemFlags();
}

bool GroupEntity::isPlanar() const
{
    return false;
}

void GroupEntity::setupHeadItemNotifications()
{
    m_headItem->setDataChangedCallback(
        [this](const QVector<int>& roles)
        {
            dataChanged(modelMapping(), roles, 0);
            if (m_callback)
                m_callback(this, roles);

            if (!m_nestedEntityCreator)
                return;

            if (std::find_first_of(std::cbegin(m_discardingRoles), std::cend(m_discardingRoles),
                std::cbegin(roles), std::cend(roles)) != std::cend(m_discardingRoles))
            {
                setNestedEntity(m_nestedEntityCreator());
            }
        });
}

const AbstractItem* GroupEntity::headItem() const
{
    return m_headItem.get();
}

void GroupEntity::setDataChangedCallback(const DataChangedCallback& callback)
{
    m_callback = callback;
}

GroupEntityPtr makeGroup(AbstractItemPtr headItem, AbstractEntityPtr nestedEntity)
{
    return std::make_unique<GroupEntity>(std::move(headItem), std::move(nestedEntity));
}

GroupEntityPtr makeGroup(AbstractItemPtr headItem)
{
    return std::make_unique<GroupEntity>(std::move(headItem));
}

GroupEntityPtr makeGroup(
    AbstractItemPtr headItem,
    const GroupEntity::EntityCreator& nestedEntityCreator,
    const QVector<int>& discardingRoles)
{
    return std::make_unique<GroupEntity>(std::move(headItem), nestedEntityCreator,
        discardingRoles);
}

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
