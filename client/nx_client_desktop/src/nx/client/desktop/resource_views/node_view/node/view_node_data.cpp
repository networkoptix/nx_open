#include "view_node_data.h"

#include <nx/client/desktop/resource_views/node_view/node_view_constants.h>
#include <nx/client/desktop/resource_views/node_view/node/view_node_data_builder.h>
#include <nx/client/desktop/resource_views/node_view/node/view_node_helpers.h>

namespace {

template<typename RoleDataHash>
void applyRoleData(const RoleDataHash& from, RoleDataHash& to)
{
    for (auto it = from.begin(); it != from.end(); ++it)
    {
        const int role = it.key();
        to[role] = it.value();
    }
}

} // namespace
namespace nx {
namespace client {
namespace desktop {

struct ViewNodeData::Private
{
    using Column = int;
    using Role = int;

    using ColumnFlagHash = QHash<Column, Qt::ItemFlags>;
    using RoleValueHash = QHash<Role, QVariant>;
    using CommonNodeDataHash = RoleValueHash;
    using ColumnDataHash = QHash<Column, RoleValueHash>;

    ColumnFlagHash flags;
    ColumnDataHash data;
    CommonNodeDataHash commonNodeData;
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
    applyRoleData(value.d->commonNodeData, d->commonNodeData);

    for (auto it = value.d->data.begin(); it != value.d->data.end(); ++it)
        applyRoleData(it.value(), d->data[it.key()]);

    for (auto it = value.d->flags.begin(); it != value.d->flags.end(); ++it)
        d->flags[it.key()] = it.value();
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

QVariant ViewNodeData::commonNodeData(int role) const
{
    const auto it = d->commonNodeData.find(role);
    return it == d->commonNodeData.end() ? QVariant() : it.value();
}

void ViewNodeData::setCommonNodeData(int role, const QVariant& data)
{
    NX_EXPECT(!data.isNull(), "Empty data is not allowed");
    d->commonNodeData[role] = data;
}

void ViewNodeData::removeCommonNodeData(int role)
{
    d->commonNodeData.remove(role);
}

ViewNodeData::Columns ViewNodeData::usedColumns() const
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
    static constexpr Qt::ItemFlags kCheckableFlags = Qt::ItemIsUserCheckable | Qt::ItemIsEditable;

    const auto flagIt = d->flags.find(column);
    const auto flagsValue = flagIt == d->flags.end() ? Qt::ItemIsEnabled : flagIt.value();
    return node_view::helpers::isCheckable(*this, column)
        ? flagsValue | kCheckableFlags
        : flagsValue;
}

} // namespace desktop
} // namespace client
} // namespace nx

