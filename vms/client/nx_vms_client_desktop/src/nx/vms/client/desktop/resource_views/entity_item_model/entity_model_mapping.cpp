// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "entity_model_mapping.h"

#include <nx/utils/log/assert.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity_item_model.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/flattening_group_entity.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/base_notification_observer.h>

namespace nx::vms::client::desktop {
namespace entity_item_model {

EntityModelMapping::~EntityModelMapping()
{
}

bool EntityModelMapping::setEntityModel(EntityItemModel* model)
{
    if (isHeadMapping() && m_entityModel != model)
    {
        m_entityModel = model;
        return true;
    }
    return false;
}

EntityItemModel* EntityModelMapping::entityItemModel() const
{
    auto mapping = this;
    while (true)
    {
        if (mapping->isHeadMapping())
            return mapping->m_entityModel;
        mapping = mapping->parentMapping();
    }
    return nullptr;
}

QModelIndex EntityModelMapping::parentModelIndex() const
{
    const auto model = entityItemModel();
    const auto widestPlanar = widestPlanarMapping();

    if (!model || widestPlanar->isHeadMapping())
        return QModelIndex();

    auto lowerLevelMapping = widestPlanar->parentMapping();
    auto lowerLevelWidestPlanar = lowerLevelMapping->widestPlanarMapping();

    const auto indexRow = lowerLevelWidestPlanar->ownerEntity()->
        childEntityRow(widestPlanar->ownerEntity());

    return model->createIndex(indexRow, 0, lowerLevelWidestPlanar->ownerEntity());
}

QModelIndex EntityModelMapping::entityModelIndex(int row) const
{
    const auto model = entityItemModel();
    if (!model)
        return QModelIndex();

    return model->createIndex(row + entityOffset(), 0, widestPlanarMapping()->ownerEntity());
}

EntityModelMapping::EntityModelMapping(AbstractEntity* entity):
    m_entity(entity)
{
}

AbstractEntity* EntityModelMapping::ownerEntity() const
{
    return m_entity;
}

EntityModelMapping* EntityModelMapping::parentMapping() const
{
    return m_parentMapping;
}

const EntityModelMapping* EntityModelMapping::widestPlanarMapping() const
{
    auto mapping = this;

    while (!mapping->isChildEntityMapping() && !mapping->isHeadMapping())
        mapping = mapping->parentMapping();

    return mapping;
}

int EntityModelMapping::entityOffset() const
{
    auto mapping = this;
    auto offset = m_offsetEvaluator ? m_offsetEvaluator(ownerEntity()) : 0;

    while (!mapping->isChildEntityMapping() && !mapping->isHeadMapping())
    {
        mapping = mapping->parentMapping();
        offset += mapping->offsetEvaluator()
            ? mapping->offsetEvaluator()(mapping->ownerEntity())
            : 0;
    }

    return offset;
}

bool EntityModelMapping::isHeadMapping() const
{
    return m_parentMapping == nullptr;
}

void EntityModelMapping::assignAsChildEntity(EntityModelMapping* parentEntityMapping)
{
    NX_ASSERT(parentEntityMapping, "Parent mapping should be defined");

    if (!parentEntityMapping)
        return;

    m_parentMapping = parentEntityMapping;
    m_offsetEvaluator = OffsetEvaluator();
    m_isChildEntityMapping = true;
}

bool EntityModelMapping::isChildEntityMapping() const
{
    return m_isChildEntityMapping;
}

void EntityModelMapping::assignAsSubEntity(
    EntityModelMapping* parentEntityMapping,
    OffsetEvaluator offsetEvaluator)
{
    NX_ASSERT(parentEntityMapping, "Parent mapping should be defined");
    NX_ASSERT(offsetEvaluator, "Subentity offset evaluator should be defined");

    if (!parentEntityMapping || !offsetEvaluator)
        return;

    m_parentMapping = parentEntityMapping;
    m_offsetEvaluator = offsetEvaluator;
    m_isChildEntityMapping = false;
}

bool EntityModelMapping::isSubEntityMapping() const
{
    return !isChildEntityMapping() && !isHeadMapping();
}

EntityModelMapping::OffsetEvaluator EntityModelMapping::offsetEvaluator() const
{
    return m_offsetEvaluator;
}

void EntityModelMapping::unassignEntity()
{
    m_entityModel = nullptr;
    m_parentMapping = nullptr;
    m_isChildEntityMapping = false;
    m_offsetEvaluator = OffsetEvaluator();
}

void EntityModelMapping::setNotificationObserver(
    std::unique_ptr<BaseNotificatonObserver> notificationObserver)
{
    m_notificationObserver = std::move(notificationObserver);
}

BaseNotificatonObserver* EntityModelMapping::notificationObserver()
{
    return m_notificationObserver.get();
}

void EntityModelMapping::dataChanged(int first, int last, const QVector<int>& roles)
{
    const auto model = entityItemModel();
    const auto thisOffset = entityOffset();

    if (model)
    {
        const auto entity = widestPlanarMapping()->ownerEntity();

        QModelIndex topLeft = model->createIndex(first + thisOffset, 0, entity);
        QModelIndex bottomRight = model->createIndex(last + thisOffset, 0, entity);

        model->dataChanged(topLeft, bottomRight, roles);
    }

    if (notificationObserver())
        notificationObserver()->dataChanged(first, last, roles);
}

void EntityModelMapping::beginInsertRows(int first, int last)
{
    const auto model = entityItemModel();
    const auto thisOffset = entityOffset();

    if (model)
        model->beginInsertRows(parentModelIndex(), first + thisOffset, last + thisOffset);

    auto mapping = this;
    while (true)
    {
        if (auto observer = mapping->notificationObserver())
        {
            const auto offset = (mapping == this) ? 0 : thisOffset - mapping->entityOffset();
            observer->beginInsertRows(first + offset, last + offset);
        }

        if (!mapping->isSubEntityMapping())
            break;

        mapping = mapping->parentMapping();
    }
}

void EntityModelMapping::endInsertRows()
{
    const auto model = entityItemModel();
    if (model)
        model->endInsertRows();

    auto mapping = this;
    while (true)
    {
        if (const auto observer = mapping->notificationObserver())
            observer->endInsertRows();

        if (!mapping->isSubEntityMapping())
            break;

        mapping = mapping->parentMapping();
    }
}

void EntityModelMapping::beginRemoveRows(int first, int last)
{
    const auto model = entityItemModel();
    const auto thisOffset = entityOffset();

    if (model)
        model->beginRemoveRows(parentModelIndex(), first + thisOffset, last + thisOffset);

    auto mapping = this;
    while (true)
    {
        if (const auto observer = mapping->notificationObserver())
        {
            const auto offset = (mapping == this) ? 0 : thisOffset - mapping->entityOffset();
            observer->beginRemoveRows(first + offset, last + offset);
        }

        if (!mapping->isSubEntityMapping())
            break;

        mapping = mapping->parentMapping();
    }
}

void EntityModelMapping::endRemoveRows()
{
    const auto model = entityItemModel();
    if (model)
        model->endRemoveRows();

    auto mapping = this;
    while (true)
    {
        if (const auto observer = mapping->notificationObserver())
            observer->endRemoveRows();

        if (!mapping->isSubEntityMapping())
            break;

        mapping = mapping->parentMapping();
    }
}

void EntityModelMapping::beginMoveRows(
    const AbstractEntity* source, int start, int end,
    const AbstractEntity* destination, int row)
{
    const auto model = entityItemModel();

    if (model)
    {
        const auto sourceMapping = source->modelMapping();
        const auto sourceParentIndex = sourceMapping->parentModelIndex();
        const int sourceOffset = sourceMapping->entityOffset();

        const auto destinationMapping = destination->modelMapping();
        auto destinationParentIndex = destinationMapping->parentModelIndex();
        int destinationOffset = destinationMapping->entityOffset();

        // When beginRemoveRows() is called there are no changes should be done in the model,
        // so model mapping hasn't updated at this moment. This workaround is needed to get right
        // index in this state. TODO: #vbreus Figure out how to get rid of it.
        if (row == 0 && dynamic_cast<const FlatteningGroupEntity*>(destination))
        {
            destinationParentIndex = destinationMapping->entityModelIndex(row);
            destinationOffset = 0;
        }

        model->beginMoveRows(
            sourceParentIndex, start + sourceOffset, end + sourceOffset,
            destinationParentIndex, row + destinationOffset);
    }

    auto mapping = this;
    while (true)
    {
        if (const auto observer = mapping->notificationObserver())
            observer->beginMoveRows(source, start, end, destination, row);

        if (!mapping->isSubEntityMapping())
            break;

        mapping = mapping->parentMapping();
    }
}

void EntityModelMapping::endMoveRows()
{
    const auto model = entityItemModel();

    if (model)
        model->endMoveRows();

    auto mapping = this;
    while (true)
    {
        if (const auto observer = mapping->notificationObserver())
            observer->endMoveRows();

        if (!mapping->isSubEntityMapping())
            break;

        mapping = mapping->parentMapping();
    }
}

void EntityModelMapping::layoutAboutToBeChanged(const std::vector<int>& indexPermutation)
{
    const auto model = entityItemModel();

    if (!model)
        return;

    model->layoutAboutToBeChanged({parentModelIndex()}, QAbstractItemModel::VerticalSortHint);

    QModelIndexList fromList;
    QModelIndexList toList;
    for (int i = 0; i < indexPermutation.size(); ++i)
    {
        fromList.append(entityModelIndex(i));
        toList.append(entityModelIndex(indexPermutation[i]));
    }
    model->changePersistentIndexList(fromList, toList);
}

void EntityModelMapping::layoutChanged()
{
    const auto model = entityItemModel();

    if (model)
        model->layoutChanged({parentModelIndex()}, QAbstractItemModel::VerticalSortHint);
}

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
