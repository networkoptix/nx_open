// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "lookup_list_entries_model.h"

#include <set>

#include <QtCore/QString>

#include <nx/utils/range_adapters.h>
#include <ui/utils/table_export_helper.h>

#include "private/lookup_list_entries_model_p.h"

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
    base_type(parent),
    d(new Private())
{
}

LookupListEntriesModel::~LookupListEntriesModel()
{
}

QVariant LookupListEntriesModel::headerData(
    int section, Qt::Orientation orientation, int role) const
{
    if (!NX_ASSERT(d->data))
        return {};

    if (orientation != Qt::Orientation::Horizontal)
        return QVariant();

    if (role == Qt::DisplayRole)
        return attribute(d->data->rawData(), section);

    return QVariant();
}

int LookupListEntriesModel::rowCount(const QModelIndex& /*parent*/) const
{
    if (!d->data)
        return 0;

    return d->data->rawData().entries.size();
}

int LookupListEntriesModel::columnCount(const QModelIndex& parent) const
{
    // Reserve one for checkbox column.
    return d->data ? (int) d->data->rawData().attributeNames.size() : 0;
}

QVariant LookupListEntriesModel::data(const QModelIndex& index, int role) const
{
    if (!NX_ASSERT(d->data))
        return {};

    switch (role)
    {
        case RawValueRole:
        case Qt::DisplayRole:
        {
            const auto key = attribute(d->data->rawData(), index.column());
            if (key.isEmpty())
                return QString();

            const auto& entry = d->data->rawData().entries[index.row()];
            const auto iter = entry.find(key);
            const auto value = iter != entry.cend() ? iter->second : QString();
            return role == RawValueRole ? value : d->getDisplayValue(key, value);
        }

        case ObjectTypeIdRole:
        {
            return d->data->rawData().objectTypeId;
        }

        case AttributeNameRole:
        {
            return attribute(d->data->rawData(), index.column());
        }
    }

    return {};
}

bool LookupListEntriesModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!NX_ASSERT(d->data))
        return false;
    if (role != Qt::EditRole)
        return false;


    const auto key = attribute(d->data->rawData(), index.column());
    if (key.isEmpty())
        return false;

    auto& entry = d->data->rawData().entries[index.row()];
    auto iter = entry.find(key);

    const auto valueAsString = value.toString();
    if (valueAsString.isEmpty() && iter != entry.end())
        entry.erase(iter);
    else
        entry[key] = valueAsString;

    emit dataChanged(index, index, {Qt::DisplayRole, RawValueRole});
    return true;
}

QHash<int, QByteArray> LookupListEntriesModel::roleNames() const
{
    auto roles = base_type::roleNames();
    roles[ObjectTypeIdRole] = "objectTypeId";
    roles[AttributeNameRole] = "attributeName";
    roles[RawValueRole] = "rawValue";
    return roles;
}

LookupListModel* LookupListEntriesModel::listModel() const
{
    return d->data.data();
}

void LookupListEntriesModel::setListModel(LookupListModel* value)
{
    beginResetModel();
    if (d->data)
        d->data->disconnect(this);

    d->data = value;
    endResetModel();
    d->initAttributeFunctions();
    emit listModelChanged(value);

    if (d->data)
    {
        connect(d->data.get(),
            &LookupListModel::attributeNamesChanged,
            this,
            &LookupListEntriesModel::removeIncorrectEntries);
        emit headerDataChanged(Qt::Orientation::Horizontal, 0, columnCount() - 1);
        emit rowCountChanged();
    }
}

void LookupListEntriesModel::addEntry(const QVariantMap& values)
{
    if (!NX_ASSERT(d->data))
        return;

    if (!NX_ASSERT(!values.empty()))
        return;

    const int count = d->data->rawData().entries.size();
    beginInsertRows({}, count, count);

    nx::vms::api::LookupListEntry entry;
    for (const auto& [name, value]: nx::utils::constKeyValueRange(values))
        entry[name] = value.toString();
    d->data->rawData().entries.push_back(std::move(entry));

    endInsertRows();
    emit rowCountChanged();
}

void LookupListEntriesModel::deleteEntries(const QVector<int>& rows)
{
    if (!NX_ASSERT(d->data))
        return;

    if (rows.empty())
        return;

    const std::set<int, std::greater<>> uniqueValuesInDescendingOrder(rows.begin(), rows.end());
    beginResetModel();
    for (const auto& index: uniqueValuesInDescendingOrder)
    {
        if (index >= d->data->rawData().entries.size())
            continue;
        d->data->rawData().entries.erase(d->data->rawData().entries.begin() + index);
    }
    endResetModel();
    emit rowCountChanged();
}

void LookupListEntriesModel::exportEntries(const QSet<int>& selectedRows, QTextStream& outputCsv)
{
    if (!NX_ASSERT(d->data))
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

    QnTableExportHelper::exportToStreamCsv(this, indexes, outputCsv, RawValueRole);
}

bool LookupListEntriesModel::isValidValue(const QString& value, const QString& attributeName)
{
    return d->isValidValue(value, attributeName);
}

core::analytics::taxonomy::StateView* LookupListEntriesModel::taxonomy()
{
    return d->taxonomy;
}

void LookupListEntriesModel::setTaxonomy(core::analytics::taxonomy::StateView* taxonomy)
{
    d->taxonomy = taxonomy;
    d->initAttributeFunctions();
    emit taxonomyChanged();
}

int LookupListEntriesModel::columnPosOfAttribute(const QString& attributeName)
{
    if (!d->data)
        return -1;

    const auto& attributeNames = d->data->rawData().attributeNames;
    const auto pos = std::find(attributeNames.begin(), attributeNames.end(), attributeName);
    return pos != attributeNames.end() ? std::distance(attributeNames.begin(), pos) : -1;
}

void LookupListEntriesModel::removeIncorrectEntries()
{
    if (!NX_ASSERT(d->data, "Function must never be called on uninitialized model"))
        return;

    // Remove remaining empty rows, after column values validation.
    deleteEntries(d->removeIncorrectColumnValues());
}

void LookupListEntriesModel::update()
{
    beginResetModel();
    endResetModel();
    emit rowCountChanged();
}

} // namespace nx::vms::client::desktop
