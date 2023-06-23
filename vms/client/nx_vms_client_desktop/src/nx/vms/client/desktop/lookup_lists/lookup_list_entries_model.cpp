// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "lookup_list_entries_model.h"

#include <nx/utils/range_adapters.h>

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

LookupListEntriesModel::LookupListEntriesModel(QObject* parent):
    base_type(parent)
{
    for (int i = 0; i < m_checkBoxCount_TEMP; ++i)
        m_checkBoxState_TEMP[i] = Qt::Unchecked;
}

LookupListEntriesModel::~LookupListEntriesModel()
{
}

QVariant LookupListEntriesModel::headerData(
    int section, Qt::Orientation orientation, int role) const
{
    if (!NX_ASSERT(m_data))
        return {};

    if (orientation != Qt::Orientation::Horizontal)
        return QVariant();

    if (section == kCheckBoxColumnIndex)
    {
        if (role == Qt::CheckStateRole)
            return Qt::Unchecked;
    }
    else
    {
        if (role == Qt::DisplayRole)
            return attribute(m_data->rawData(), section);
    }

    return QVariant();
}

int LookupListEntriesModel::rowCount(const QModelIndex& parent) const
{
    return m_data ? (int) m_data->rawData().entries.size() : 0;
}

int LookupListEntriesModel::columnCount(const QModelIndex& parent) const
{
    // Reserve one for checkbox column.
    return m_data ? (int) m_data->rawData().attributeNames.size() + 1 : 0;
}

QVariant LookupListEntriesModel::data(const QModelIndex& index, int role) const
{
    if (!NX_ASSERT(m_data))
        return {};

    switch (role)
    {
        case Qt::DisplayRole:
        {
            if (index.column() == kCheckBoxColumnIndex)
            {
                if (index.row() < m_checkBoxCount_TEMP)
                    return m_checkBoxState_TEMP[index.row()];
            }

            const auto key = attribute(m_data->rawData(), index.column());
            if (key.isEmpty())
                return QString();

            const auto& entry = m_data->rawData().entries[index.row()];
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

bool LookupListEntriesModel::setData(
    const QModelIndex& index,
    const QVariant& value,
    int role)
{
    if (!NX_ASSERT(m_data))
        return {};

    switch (role)
    {
        case Qt::EditRole:
        {
            if (index.column() == kCheckBoxColumnIndex)
            {
                if (index.row() < m_checkBoxCount_TEMP)
                {
                    m_checkBoxState_TEMP[index.row()] = value.toInt();
                    emit dataChanged(index, index, {Qt::DisplayRole});
                    return true;
                }
            }
        }
    }

    return QAbstractTableModel::setData(index, value, role);
}

QHash<int, QByteArray> LookupListEntriesModel::roleNames() const
{
    auto roles = base_type::roleNames();
    roles[TypeRole] = "type";
    return roles;
}

LookupListModel* LookupListEntriesModel::listModel() const
{
    return m_data.data();
}

void LookupListEntriesModel::setListModel(LookupListModel* value)
{
    beginResetModel();
    m_data = value;
    endResetModel();

    emit listModelChanged();

    if (m_data)
        emit headerDataChanged(Qt::Orientation::Horizontal, 0, columnCount() - 1);
}

void LookupListEntriesModel::addEntry(const QVariantMap& value)
{
    if (!NX_ASSERT(m_data))
        return;

    if (!NX_ASSERT(!value.empty()))
        return;

    const int count = m_data->rawData().entries.size();
    beginInsertRows({}, count, count);

    nx::vms::api::LookupListEntry entry;
    for (const auto& [name, value]: nx::utils::constKeyValueRange(value))
        entry[name] = value.toString();
    m_data->rawData().entries.push_back(std::move(entry));

    endInsertRows();
}

} // namespace nx::vms::client::desktop
