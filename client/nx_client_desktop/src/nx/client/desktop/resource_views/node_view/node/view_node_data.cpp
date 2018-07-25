#include "view_node_data.h"

#include <nx/client/desktop/resource_views/node_view/node_view_constants.h>
#include <nx/client/desktop/resource_views/node_view/node/view_node_data_builder.h>

namespace nx {
namespace client {
namespace desktop {

struct ViewNodeData::Private
{
    using Column = int;
    using Role = int;

    using ColumnFlagHash = QHash<Column, Qt::ItemFlags>;
    using RoleValueHash = QHash<Role, QVariant>;
    using GenericDataHash = RoleValueHash;
    using ColumnDataHash = QHash<Column, RoleValueHash>;

    ColumnFlagHash flags;
    ColumnDataHash data;
    GenericDataHash genericData;
};

//-------------------------------------------------------------------------------------------------

ViewNodeData::ViewNodeData():
    d(new Private())
{
}

ViewNodeData::ViewNodeData(const ViewNodeData& other):
    d(new Private(*other.d))
{
}

ViewNodeData::ViewNodeData(const ViewNodeDataBuilder& builder):
    d(new Private(*builder.data().d))
{
}

ViewNodeData::~ViewNodeData()
{
}

void ViewNodeData::applyData(const ViewNodeData& value)
{
    for (auto it = value.d->data.begin(); it != value.d->data.end(); ++it)
    {
        const int column = it.key();
        const auto& roleData = it.value();
        for (auto itRoleData = roleData.begin(); itRoleData != roleData.end(); ++itRoleData)
        {
            const int role = itRoleData.key();
            d->data[column][role] = itRoleData.value();
        }
    }

    for (auto it = value.d->genericData.begin(); it != value.d->genericData.end(); ++it)
    {
        const int role = it.key();
        d->genericData[role] = it.value();
    }

    for (auto it = value.d->flags.begin(); it != value.d->flags.end(); ++it)
        d->flags[it.key()] = it.value();
}

bool ViewNodeData::hasData(int column, int role) const
{
    const auto it = d->data.find(column);
    if (it == d->data.end())
        return false;

    const auto& valueHash = it.value();
    const auto itValue = valueHash.find(role);
    if (itValue == valueHash.end())
        return false;

    const bool isEmptyValue = itValue.value().isNull();
    NX_EXPECT(!isEmptyValue, "Empty values are not accepted");
    return !isEmptyValue;
}

bool ViewNodeData::hasDataForColumn(int column) const
{
    const auto it = d->data.find(column);
    if (it == d->data.end())
        return false;

    const auto& valueHash = it.value();
    if (valueHash.isEmpty())
        return false;

    const bool result = std::any_of(valueHash.begin(), valueHash.end(),
        [](const QVariant& value) { return !value.isNull(); });

    NX_EXPECT(!std::any_of(valueHash.begin(), valueHash.end(),
        [](const QVariant& value) { return value.isNull(); }),
        "Empty values are not accepted");

    return result;
}

QVariant ViewNodeData::data(int column, int role) const
{
    const auto columnIt = d->data.find(column);
    if (columnIt == d->data.end())
        return QVariant();

    const auto& columnData = columnIt.value();
    const auto dataIt = columnData.find(role);
    if (dataIt == columnData.end())
        return QVariant();

    return dataIt.value();
}

void ViewNodeData::setData(int column, int role, const QVariant& data)
{
    NX_EXPECT(!data.isNull(), "Empty data is not allowed");
    d->data[column][role] = data;
}

void ViewNodeData::removeData(int column, int role)
{
    const auto it = d->data.find(column);
    if (it != d->data.end())
        it.value().remove(role);
}

QVariant ViewNodeData::genericData(int role) const
{
    const auto it = d->genericData.find(role);
    return it == d->genericData.end() ? QVariant() : it.value();
}

void ViewNodeData::setGenericData(int role, const QVariant& data)
{
    d->genericData[role] = data;
}

void ViewNodeData::removeGenericData(int role)
{
    d->genericData.remove(role);
}

ViewNodeData::Columns ViewNodeData::columns() const
{
    return d->data.keys();
}

ViewNodeData::Roles ViewNodeData::rolesForColumn(int column) const
{
    const auto it = d->data.find(column);
    return it == d->data.end() ? Roles() : it.value().keys().toVector();
}

Qt::ItemFlags ViewNodeData::flags(int column) const
{
    const auto flagIt = d->flags.find(column);
    if (flagIt == d->flags.end())
        return Qt::ItemIsEnabled;

    static constexpr Qt::ItemFlags kCheckableFlags = Qt::ItemIsUserCheckable | Qt::ItemIsEditable;
    return column == node_view::checkMarkColumn
        ? flagIt.value() | kCheckableFlags
        : flagIt.value();
}

} // namespace desktop
} // namespace client
} // namespace nx

