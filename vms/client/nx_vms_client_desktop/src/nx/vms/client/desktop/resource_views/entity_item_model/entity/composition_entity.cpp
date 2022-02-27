// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "composition_entity.h"

#include <nx/utils/log/assert.h>
#include <client/client_globals.h>

#include <nx/vms/client/desktop/resource_views/entity_item_model/entity_model_mapping.h>
#include <nx/vms/client/desktop/resource_views/entity_item_model/entity/entity_notification_guard.h>

namespace nx::vms::client::desktop {
namespace entity_item_model {

CompositionEntity::CompositionEntity():
    base_type()
{
}

CompositionEntity::~CompositionEntity()
{
    resetNotificationObserver();
    auto guard = removeRowsGuard(modelMapping(), 0, rowCount());
    for (auto& subEntity: m_composition)
        unassignEntity(subEntity.get());
    m_composition.clear();
}

int CompositionEntity::rowCount() const
{
    if (m_isHidden)
        return 0;

    return std::accumulate(m_composition.cbegin(), m_composition.cend(), 0,
        [](int sum, const AbstractEntityPtr& subEnt) { return subEnt->rowCount() + sum; });
}

AbstractEntity* CompositionEntity::childEntity(int row) const
{
    const auto subEntityWithRow = subEntityWithLocalRow(row);
    return subEntityWithRow.first->childEntity(subEntityWithRow.second);
}

int CompositionEntity::childEntityRow(const AbstractEntity* entity) const
{
    int offset = 0;
    for (const auto& subEntity: m_composition)
    {
        if (!subEntity->isPlanar())
        {
            const auto subEntityChildEntityRow = subEntity->childEntityRow(entity);
            if (subEntityChildEntityRow != -1)
                return subEntityChildEntityRow + offset;
        }
        offset += subEntity->rowCount();
    }
    return -1;
}

QVariant CompositionEntity::data(int row, int role /*= Qt::DisplayRole*/) const
{
    const auto subEntityWithRow = subEntityWithLocalRow(row);
    if (!NX_ASSERT(subEntityWithRow.first))
        return {};

    auto result = subEntityWithRow.first->data(subEntityWithRow.second, role);
    return result;
}

Qt::ItemFlags CompositionEntity::flags(int row) const
{
    const auto subEntityWithRow = subEntityWithLocalRow(row);
    return subEntityWithRow.first->flags(subEntityWithRow.second);
}

void CompositionEntity::addSubEntity(AbstractEntityPtr subEntity)
{
    auto guard = insertRowsGuard(modelMapping(), rowCount(), subEntity->rowCount());

    assignAsSubEntity(subEntity.get(),
        [this](const AbstractEntity* subEntity) { return subEntityOffset(subEntity); });

    m_composition.emplace_back(std::move(subEntity));
}

void CompositionEntity::removeSubEntity(const AbstractEntity* subEntityToRemove)
{
    auto itr = std::find_if(std::begin(m_composition), std::end(m_composition),
        [subEntityToRemove](const AbstractEntityPtr& subEntity)
        {
            return subEntityToRemove == subEntity.get();
        });

    if (itr == std::end(m_composition))
        return;

    auto guard = removeRowsGuard(modelMapping(),
        subEntityOffset(subEntityToRemove), subEntityToRemove->rowCount());

    unassignEntity(subEntityToRemove);

    m_composition.erase(itr);
}

int CompositionEntity::subEntityCount() const
{
    return static_cast<int>(m_composition.size());
}

AbstractEntity* CompositionEntity::subEntityAt(int index)
{
    if (index >= m_composition.size() || index < 0)
    {
        NX_ASSERT(false, "Invalid index");
        return nullptr;
    }

    return m_composition.at(index).get();
}

AbstractEntity* CompositionEntity::lastSubentity()
{
    if (m_composition.empty())
    {
        NX_ASSERT(false, "Attempt to access empty storage");
        return nullptr;
    }

    return m_composition.back().get();
}

std::pair<AbstractEntity*, int> CompositionEntity::subEntityWithLocalRow(int row) const
{
    int subentityRow = row;
    for (const auto& subEntity: m_composition)
    {
        const auto subEntityRowCount = subEntity->rowCount();
        if (subEntityRowCount > subentityRow)
            return std::make_pair(subEntity.get(), subentityRow);
        else
            subentityRow -= subEntityRowCount;
    }
    return {};
}

int CompositionEntity::subEntityOffset(const AbstractEntity* subEntity) const
{
    int result = 0;
    for (const auto& storedSubEntity: m_composition)
    {
        if (storedSubEntity.get() == subEntity)
            return result;
        result += storedSubEntity->rowCount();
    }
    NX_ASSERT(false, "Invalid argument");
    return result;
}

bool CompositionEntity::isPlanar() const
{
    return false;
}

void CompositionEntity::hide()
{
    m_hiddenRowsCount = rowCount();
    auto guard = removeRowsGuard(modelMapping(), 0, m_hiddenRowsCount);
    m_isHidden = true;
}

void CompositionEntity::show()
{
    auto guard = insertRowsGuard(modelMapping(), 0, m_hiddenRowsCount);
    m_hiddenRowsCount = 0;
    m_isHidden = false;
}

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
