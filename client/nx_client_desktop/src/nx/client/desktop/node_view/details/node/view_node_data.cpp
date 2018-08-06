#include "view_node_data.h"

#include "view_node_helpers.h"
#include "view_node_data_builder.h"

namespace {

template<typename RoleDataHash>
void applyDataInternal(const RoleDataHash& from, RoleDataHash& to)
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

bool ViewNodeData::hasData(int column, int role) const
{
    const auto it = d->data.find(column);
    if (it == d->data.end())
        return false;

    const auto values = it.value();
    const auto itData = values.find(role);
    return itData != values.end();
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

bool ViewNodeData::hasProperty(int id) const
{
    return d->properties.contains(id);
}

QVariant ViewNodeData::property(int id) const
{
    const auto it = d->properties.find(id);
    return it == d->properties.end() ? QVariant() : it.value();
}

void ViewNodeData::setProperty(int id, const QVariant& data)
{
    NX_EXPECT(!data.isNull(), "Empty data is not allowed");
    d->properties[id] = data;
}

void ViewNodeData::removeProperty(int id)
{
    d->properties.remove(id);
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
    return checkable(*this, column) ? flagsValue | kCheckableFlags : flagsValue;
}

void ViewNodeData::setFlag(int column, Qt::ItemFlag flag, bool value)
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

void ViewNodeData::setFlags(int column, Qt::ItemFlags flags)
{
    d->flags[column] = flags;
}

} // namespace details
} // namespace node_view
} // namespace desktop
} // namespace client
} // namespace nx

