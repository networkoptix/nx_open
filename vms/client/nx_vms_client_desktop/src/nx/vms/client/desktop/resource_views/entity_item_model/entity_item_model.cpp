// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "entity_item_model.h"

#include <algorithm>

#include <nx/utils/log/assert.h>
#include <client/client_globals.h>

#include <nx/vms/client/desktop/resource_views/entity_item_model/entity_model_mapping.h>

namespace nx::vms::client::desktop {
namespace entity_item_model {

EntityItemModel::EntityItemModel(int columnCount):
    base_type(nullptr),
    m_columnCount(std::max(columnCount, 1))
{
    NX_ASSERT(columnCount > 0, "Invalide column count provided, resulting model has one column");
}

EntityItemModel::~EntityItemModel()
{
    if (m_rootEntity)
        m_rootEntity->modelMapping()->setEntityModel(nullptr);
}

int EntityItemModel::rowCount(const QModelIndex& parent) const
{
    if (!m_rootEntity)
        return 0;

    if (!parent.isValid())
        return m_rootEntity->rowCount();

    if (parent.model() != this)
    {
        NX_ASSERT(false, "Invalid parent index");
        return 0;
    }

    if (parent.column() > 0)
        return 0;

    return static_cast<AbstractEntity*>(parent.internalPointer())->childRowCount(parent.row());
}

int EntityItemModel::columnCount(const QModelIndex&) const
{
    return m_columnCount;
}

QVariant EntityItemModel::data(const QModelIndex& index, int role) const
{
    if (!m_rootEntity || !index.isValid())
        return {};

    if (index.model() != this)
    {
        NX_ASSERT(false, "Foreign model index");
        return {};
    }

    if (index.column() > 0)
        return {};

    return static_cast<AbstractEntity*>(index.internalPointer())->data(index.row(), role);
}

Qt::ItemFlags EntityItemModel::flags(const QModelIndex& index) const
{
    if (!m_rootEntity || !index.isValid())
        return {};

    if (index.model() != this)
    {
        NX_ASSERT(false, "Foreign model index");
        return {};
    }

    return static_cast<AbstractEntity*>(index.internalPointer())->flags(index.row());
}

QModelIndex EntityItemModel::index(int row, int column, const QModelIndex& parent) const
{
    if (!m_rootEntity)
        return {};

    if (!parent.isValid())
        return createIndex(row, column, m_rootEntity);

    if (parent.model() != this)
    {
        NX_ASSERT(false, "Foreign model index");
        return {};
    }

    const auto parentIndexEntity = static_cast<const AbstractEntity*>(parent.internalPointer());
    const auto childEntity = parentIndexEntity->childEntity(parent.row());
    if (childEntity == nullptr)
        return {};

    return createIndex(row, column, childEntity);
}

QModelIndex EntityItemModel::parent(const QModelIndex& index) const
{
    if (!m_rootEntity || !index.isValid())
        return {};

    if (index.model() != this)
    {
        NX_ASSERT(false, "Foreign model index");
        return {};
    }

    const auto* indexEntity = static_cast<const AbstractEntity*>(index.internalPointer());
    if (indexEntity == m_rootEntity)
        return {};

    return indexEntity->modelMapping()->parentModelIndex();
}

bool EntityItemModel::hasChildren(const QModelIndex& parent) const
{
    if (!m_rootEntity)
        return false;

    if (parent.isValid())
    {
        if (parent.model() != this)
        {
            NX_ASSERT(false, "Foreign parent index");
            return false;
        }

        if (parent.column() != 0)
            return false;

        const auto* indexEntity = static_cast<const AbstractEntity*>(parent.internalPointer());
        return !indexEntity->isPlanar() && (indexEntity->childRowCount(parent.row()) > 0);
    }

    return rowCount(parent) > 0;
}

QModelIndex EntityItemModel::sibling(int row, int column, const QModelIndex& index) const
{
    if (!m_rootEntity || !index.isValid() || index.model() != this)
    {
        NX_ASSERT("Invalid index");
        return {};
    }

    if (index.row() == row && index.column() == column)
        return index;

    if (row < 0 || column < 0)
    {
        NX_ASSERT("Invalid row / column parameters");
        return {};
    }

    const auto indexEntity = static_cast<AbstractEntity*>(index.internalPointer());
    if (column < m_columnCount && row < indexEntity->rowCount())
        return createIndex(row, column, indexEntity);

    NX_ASSERT(false, "Unexpected row / column parameters");
    return {};
}

QHash<int, QByteArray> EntityItemModel::roleNames() const
{
    auto roles = base_type::roleNames();
    roles.insert(core::ResourceRole, "resourcePtr");
    roles.insert(Qn::ResourceFlagsRole, "flags");
    roles.insert(Qn::ItemUuidRole, "uuid");
    roles.insert(core::ResourceStatusRole, "status");
    roles.insert(Qn::NodeTypeRole, "nodeType");
    roles.insert(Qn::ResourceIconKeyRole, "iconKey");
    roles.insert(Qn::ExtraInfoRole, "extraInfo");
    roles.insert(Qn::ForceExtraInfoRole, "forceExtraInfo");
    roles.insert(Qn::ResourceExtraStatusRole, "resourceExtraStatus");
    roles.insert(core::DecorationPathRole, "decorationPath");
    return roles;
}

void EntityItemModel::setRootEntity(AbstractEntity* entity)
{
    if (m_rootEntity == entity)
        return;

    if (entity && entity->modelMapping()->entityItemModel())
        entity->modelMapping()->entityItemModel()->setRootEntity(nullptr);

    beginResetModel();
    if (m_rootEntity)
        m_rootEntity->modelMapping()->setEntityModel(nullptr);
    m_rootEntity = entity;
    if (m_rootEntity)
        m_rootEntity->modelMapping()->setEntityModel(this);
    endResetModel();
}

// Please use QIdentityProxyModel derived decorator to implement this functionality.
QVariant EntityItemModel::headerData(int, Qt::Orientation, int) const
{
    return QVariant();
}

// Please use QIdentityProxyModel derived decorator to implement this functionality.
bool EntityItemModel::setHeaderData(int, Qt::Orientation, const QVariant&, int)
{
    return false;
}

void EntityItemModel::setEditDelegate(EditDelegate delegate)
{
    m_editDelegate = delegate;
}

bool EntityItemModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (role != Qt::EditRole || value.isNull() || value.toString().isEmpty())
        return false;

    if (m_editDelegate)
        return m_editDelegate(index, value);

    return false;
}

bool EntityItemModel::setItemData(const QModelIndex&, const QMap<int, QVariant>&)
{
    NX_ASSERT(false, "Unexpected attempt to alter item by EntityItemModel::setItemData() call");
    return false;
}

bool EntityItemModel::insertColumns(int, int, const QModelIndex&)
{
    NX_ASSERT(false, "Unexpected EntityItemModel::insertColumns() call");
    return false;
}

bool EntityItemModel::moveColumns(const QModelIndex&, int, int, const QModelIndex&, int)
{
    NX_ASSERT(false, "Unexpected EntityItemModel::moveColumns() call");
    return false;
}

bool EntityItemModel::removeColumns(int, int, const QModelIndex&)
{
    NX_ASSERT(false, "Unexpected EntityItemModel::removeColumns() call");
    return false;
}

bool EntityItemModel::insertRows(int, int, const QModelIndex&)
{
    NX_ASSERT(false, "Unexpected EntityItemModel::insertRows() call");
    return false;
}

bool EntityItemModel::moveRows(const QModelIndex&, int, int, const QModelIndex&, int)
{
    NX_ASSERT(false, "Unexpected EntityItemModel::moveRows() call");
    return false;
}

bool EntityItemModel::removeRows(int, int, const QModelIndex&)
{
    NX_ASSERT(false, "Unexpected EntityItemModel::removeRows() call");
    return false;
}

} // namespace entity_item_model
} // namespace nx::vms::client::desktop
