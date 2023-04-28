// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "lookup_list_model.h"

namespace nx::vms::client::desktop {

namespace {

static constexpr int kCheckBoxColumnIndex = 0;

QString attribute(const nx::vms::api::LookupListData& list, int column)
{
    if (column <= kCheckBoxColumnIndex || column > list.attributeNames.size())
        return {};

    return list.attributeNames[column - 1];
}

} // namespace

LookupListModel::LookupListModel(QObject* parent):
    base_type(parent)
{
}

LookupListModel::~LookupListModel()
{
}

QVariant LookupListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Orientation::Horizontal)
        return {};

    switch (role)
    {
        case Qt::DisplayRole:
        {
            if (section == kCheckBoxColumnIndex)
                return {};
            else
                return attribute(m_data, section);
        }
        case Qt::CheckStateRole:
        {
            if (section == kCheckBoxColumnIndex)
                return m_tempHeaderCheckBoxState;
            else
                return {};
        }
    }

    NX_ASSERT("Unexpected header role");
    return {};
}

bool LookupListModel::setHeaderData(
    int section,
    Qt::Orientation orientation,
    const QVariant& value,
    int role)
{
    if (section < 0
        || section >= columnCount()
        || orientation != Qt::Horizontal
        || role != Qt::CheckStateRole)
    {
        return false;
    }

    m_tempHeaderCheckBoxState = static_cast<Qt::CheckState>(value.toInt());

    return true;
}

int LookupListModel::rowCount(const QModelIndex& parent) const
{
    return (int) m_data.entries.size();
}

int LookupListModel::columnCount(const QModelIndex& parent) const
{
    return (int) m_data.attributeNames.size() + 1; //< Reserve one for checkbox column.
}

QVariant LookupListModel::data(const QModelIndex& index, int role) const
{
    switch (role)
    {
        case Qt::DisplayRole:
        {
            auto key = attribute(m_data, index.column());
            if (key.isEmpty())
                return QString();

            const auto& entry = m_data.entries[index.row()];
            const auto iter = entry.find(key);
            if (iter != entry.cend())
                return iter->second;
            return QString();
        }

        case TypeRole:
        {
            if (index.column() == kCheckBoxColumnIndex)
                return "checkbox";

            return "text";
        }
    }

    return {};
}

QHash<int, QByteArray> LookupListModel::roleNames() const
{
    auto roles = base_type::roleNames();
    roles[TypeRole] = "type";
    return roles;
}

void LookupListModel::resetData(nx::vms::api::LookupListData data)
{
    beginResetModel();
    m_data = std::move(data);
    endResetModel();
}

void LookupListModel::addEntry(nx::vms::api::LookupListEntry entry)
{
    const int count = m_data.entries.size();
    beginInsertRows({}, count, count);
    m_data.entries.push_back(std::move(entry));
    endInsertRows();
}

} // namespace nx::vms::client::desktop
