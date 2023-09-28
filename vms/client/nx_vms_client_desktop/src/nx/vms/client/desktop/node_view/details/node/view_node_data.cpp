// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "view_node_data.h"

#include "view_node_helper.h"
#include "view_node_data_builder.h"

#include <nx/utils/log/assert.h>
#include <nx/utils/qt_helpers.h>

namespace {

using namespace nx::vms::client::desktop::node_view::details;

template<typename RoleDataHash>
bool isDifferentData(
    const RoleDataHash& data,
    Role role,
    const QVariant& value)
{
    const auto it = data.find(role);
    return it == data.end() || it.value() != value;
}

template<typename ColumnDataHash>
bool isDifferentData(
    const ColumnDataHash& data,
    Column column,
    Role role,
    const QVariant& value)
{
    const auto it = data.find(column);
    return it == data.end() || isDifferentData(it.value(), role, value);
}

template<typename RoleDataHash>
void applyDataInternal(const RoleDataHash& from, RoleDataHash& to)
{
    for (auto it = from.begin(); it != from.end(); ++it)
    {
        const auto role = it.key();
        to[role] = it.value();
    }
}

} // namespace

namespace nx::vms::client::desktop {
namespace node_view {
namespace details {

struct ViewNodeData::Private
{
    using Column = int;
    using Role = int;

    using ColumnFlagHash = QHash<Column, Qt::ItemFlags>;
    using RoleValueHash = QHash<Role, QVariant>;
    using PropertyHash = RoleValueHash;
    using ColumnDataHash = QHash<Column, RoleValueHash>;

    ColumnFlagHash flags;
    ColumnDataHash data;
    PropertyHash properties;

    ColumnSet usedColumns() const;
    RoleVector rolesForColumn(Column column) const;
    RoleVector rolesForOverride(const ViewNodeData& other, int otherColumn) const;
};

ColumnSet ViewNodeData::Private::usedColumns() const
{
    return nx::utils::toQSet(data.keys());
}

RoleVector ViewNodeData::Private::rolesForColumn(Column column) const
{
    const auto it = data.find(column);
    return it == data.end() ? RoleVector() : it.value().keys().toVector();
}

RoleVector ViewNodeData::Private::rolesForOverride(const ViewNodeData& other, int otherColumn) const
{
    const auto otherRoles = other.rolesForColumn(otherColumn);
    if (!usedColumns().contains(otherColumn))
        return otherRoles; //< All fields from newly created column.

    RoleVector result;
    const auto currentRoles = rolesForColumn(otherColumn);
    for (const auto otherRole: otherRoles)
    {
        if (!currentRoles.contains(otherRole))
            result.append(otherRole);
    }
    return result;
}

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

ViewNodeData& ViewNodeData::operator=(const ViewNodeData& other)
{
    d.reset(new Private(*other.d));
    return *this;
}

void ViewNodeData::applyData(const ViewNodeData& value)
{
    applyDataInternal(value.d->properties, d->properties);

    for (auto it = value.d->data.begin(); it != value.d->data.end(); ++it)
        applyDataInternal(it.value(), d->data[it.key()]);

    for (auto it = value.d->flags.begin(); it != value.d->flags.end(); ++it)
        d->flags[it.key()] = it.value();
}

QVariant ViewNodeData::data(Column column, Role role) const
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

void ViewNodeData::setData(Column column, Role role, const QVariant& data)
{
    d->data[column][role] = data;
}

void ViewNodeData::removeData(Column column, Role role)
{
    const auto it = d->data.find(column);
    if (it != d->data.end())
        it.value().remove(role);
}

void ViewNodeData::removeData(const ColumnRoleHash& rolesHash)
{
    for (const Column column: rolesHash.keys())
    {
        const auto& roles = rolesHash.value(column);
        for (const auto role: roles)
            removeData(column, role);
    }
}

bool ViewNodeData::hasDataForColumn(Column column) const
{
    const auto it = d->data.find(column);
    if (it == d->data.end())
        return false;

    const auto& valueHash = it.value();
    if (valueHash.isEmpty())
        return false;

    const bool result = std::any_of(valueHash.begin(), valueHash.end(),
        [](const QVariant& value) { return !value.isNull(); });

    return result;
}

bool ViewNodeData::hasData(Column column, Role role) const
{
    const auto it = d->data.find(column);
    if (it == d->data.end())
        return false;

    const auto values = it.value();
    const auto itData = values.find(role);
    return itData != values.end();
}

QVariant ViewNodeData::property(int id) const
{
    const auto it = d->properties.find(id);
    return it == d->properties.end() ? QVariant() : it.value();
}

void ViewNodeData::setProperty(int id, const QVariant& data)
{
    NX_ASSERT(!data.isNull(), "Empty data is not allowed");
    d->properties[id] = data;
}

void ViewNodeData::removeProperty(int id)
{
    d->properties.remove(id);
}

bool ViewNodeData::hasProperty(int id) const
{
    return d->properties.contains(id);
}

ColumnSet ViewNodeData::usedColumns() const
{
    return d->usedColumns();
}

RoleVector ViewNodeData::rolesForColumn(Column column) const
{
    return d->rolesForColumn(column);
}

Qt::ItemFlags ViewNodeData::flags(Column column) const
{
    static constexpr Qt::ItemFlags kCheckableFlags = Qt::ItemIsUserCheckable;

    const auto flagIt = d->flags.find(column);
    const auto flagsValue = flagIt == d->flags.end() ? Qt::ItemIsEnabled : flagIt.value();
    return ViewNodeHelper(*this).checkable(column) ? flagsValue | kCheckableFlags : flagsValue;
}

void ViewNodeData::setFlag(Column column, Qt::ItemFlag flag, bool value)
{
    auto it = d->flags.find(column);
    if (it == d->flags.end())
    {
        if(value)
            d->flags.insert(column, flag);
    }
    else if (value)
    {
        it.value() |= flag;
    }
    else
    {
        it.value() &= ~flag;
    }
}

void ViewNodeData::setFlags(Column column, Qt::ItemFlags flags)
{
    d->flags[column] = flags;
}

ViewNodeData::DifferenceData ViewNodeData::difference(const ViewNodeData& other) const
{
    RemoveData forRemove;
    ViewNodeData forOverride;

    const auto otherColumns = other.usedColumns();

    // Checks for removed and changed data.
    for (const auto column: usedColumns())
    {
        const auto it = otherColumns.find(column);
        if (it == otherColumns.end())
        {
            // There is no such column at the "other" side, remove all data from it.
            forRemove.insert(column, rolesForColumn(column));
            continue;
        }

        const auto otherRolesAtColumn = other.rolesForColumn(column);
        for (const auto role: rolesForColumn(column))
        {
            if (!otherRolesAtColumn.contains(role))
            {
                // Data under <column, role> is deleted.
                forRemove[column].append(role);
                continue;
            }

            const auto otherData = other.data(column, role);
            if (data(column, role) != otherData)
                forOverride.setData(column, role, otherData);
        }
    }

    // Checks for newly added data fields.
    for (const auto otherColumn: otherColumns)
    {
        for (const auto role: d->rolesForOverride(other, otherColumn))
            forOverride.setData(otherColumn, role, other.data(otherColumn, role));
    }

    return {
        {PatchStepOperation::removeDataOperation, QVariant::fromValue(forRemove)},
        {PatchStepOperation::updateDataOperation, QVariant::fromValue(forOverride)}
    };
}

} // namespace details
} // namespace node_view
} // namespace nx::vms::client::desktop
