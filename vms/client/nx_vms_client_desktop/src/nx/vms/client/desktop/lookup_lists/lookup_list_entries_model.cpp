// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "lookup_list_entries_model.h"

#include <range/v3/view/reverse.hpp>

#include <nx/utils/range_adapters.h>
#include <ui/utils/table_export_helper.h>

namespace nx::vms::client::desktop {

namespace {

QString attribute(const nx::vms::api::LookupListData& list, int column)
{
    if (column >= list.attributeNames.size())
        return {};

    return list.attributeNames[column];
}

} // namespace

LookupListEntriesModel::LookupListEntriesModel(QObject* parent):
    base_type(parent)
{
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

    if (role == Qt::DisplayRole)
        return attribute(m_data->rawData(), section);

    return QVariant();
}

int LookupListEntriesModel::rowCount(const QModelIndex& parent) const
{
    return m_data ? (int) m_data->rawData().entries.size() : 0;
}

int LookupListEntriesModel::columnCount(const QModelIndex& parent) const
{
    // Reserve one for checkbox column.
    return m_data ? (int) m_data->rawData().attributeNames.size() : 0;
}

QVariant LookupListEntriesModel::data(const QModelIndex& index, int role) const
{
    if (!NX_ASSERT(m_data))
        return {};

    switch (role)
    {
        case Qt::DisplayRole:
        {
            const auto key = attribute(m_data->rawData(), index.column());
            if (key.isEmpty())
                return QString();

            const auto& entry = m_data->rawData().entries[index.row()];
            const auto iter = entry.find(key);
            if (iter != entry.cend())
                return iter->second;
            return QString();
        }

        case ObjectTypeIdRole:
        {
            return m_data->rawData().objectTypeId;
        }

        case AttributeNameRole:
        {
            return attribute(m_data->rawData(), index.column());
        }
    }

    return {};
}

bool LookupListEntriesModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!NX_ASSERT(m_data))
        return false;
    if (role != Qt::EditRole)
        return false;


    const auto key = attribute(m_data->rawData(), index.column());
    if (key.isEmpty())
        return false;

    auto& entry = m_data->rawData().entries[index.row()];
    auto iter = entry.find(key);

    const auto valueAsString = value.toString();
    if (valueAsString.isEmpty() && iter != entry.end())
        entry.erase(iter);
    else
        entry[key] = valueAsString;

    emit dataChanged(index, index, {Qt::DisplayRole});
    return true;
}

QHash<int, QByteArray> LookupListEntriesModel::roleNames() const
{
    auto roles = base_type::roleNames();
    roles[ObjectTypeIdRole] = "objectTypeId";
    roles[AttributeNameRole] = "attributeName";
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

void LookupListEntriesModel::addEntry(const QVariantMap& values)
{
    if (!NX_ASSERT(m_data))
        return;

    if (!NX_ASSERT(!values.empty()))
        return;

    const int count = m_data->rawData().entries.size();
    beginInsertRows({}, count, count);

    nx::vms::api::LookupListEntry entry;
    for (const auto& [name, value]: nx::utils::constKeyValueRange(values))
        entry[name] = value.toString();
    m_data->rawData().entries.push_back(std::move(entry));

    endInsertRows();
}

void LookupListEntriesModel::deleteEntries(const QVector<int>& rows)
{
    if (!NX_ASSERT(m_data))
        return;

    if (!NX_ASSERT(!rows.empty()))
        return;

    for (auto& index: rows | ranges::views::reverse)
    {
        beginRemoveRows({}, index, index);
        m_data->rawData().entries.erase(m_data->rawData().entries.begin() + index);
        endRemoveRows();
    }
}

void LookupListEntriesModel::exportEntries(const QSet<int>& selectedRows, QTextStream& outputCsv)
{
    if (!NX_ASSERT(m_data))
        return;

    QModelIndexList indexes;
    if (selectedRows.isEmpty() || selectedRows.size() == rowCount())
    {
        indexes = QnTableExportHelper::getAllIndexes(this);
    }
    else
    {
        indexes = QnTableExportHelper::getFilteredIndexes(
            this, [&](const QModelIndex& index) { return selectedRows.contains(index.row()); });
    }

    QnTableExportHelper::exportToStreamCsv(this, indexes, outputCsv);
}

} // namespace nx::vms::client::desktop
