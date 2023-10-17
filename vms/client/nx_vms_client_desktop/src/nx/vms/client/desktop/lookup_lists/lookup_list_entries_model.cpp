// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "lookup_list_entries_model.h"

#include <range/v3/view/reverse.hpp>

#include <QtCore/QString>
#include <QtCore/QStringLiteral>

#include <nx/utils/range_adapters.h>
#include <ui/utils/table_export_helper.h>

// TODO: @pprivalov this is an entrance point when you start to deal with validate
// #include <nx/vms/client/desktop/analytics/analytics_taxonomy_manager.h>

namespace nx::vms::client::desktop {

namespace {

QString attribute(const nx::vms::api::LookupListData& list, int column)
{
    if (column >= list.attributeNames.size())
        return {};

    return list.attributeNames[column];
}

} // namespace

struct LookupListEntriesModel::Private
{
    QPointer<LookupListModel> data;
    std::optional<QList<int>> rowsIndexesToShow;
};

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

int LookupListEntriesModel::rowCount(const QModelIndex& parent) const
{
    if (!NX_ASSERT(d->data))
        return 0;

    return d->rowsIndexesToShow ? d->rowsIndexesToShow->size() : d->data->rawData().entries.size();
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
        case Qt::DisplayRole:
        {
            const auto key = attribute(d->data->rawData(), index.column());
            if (key.isEmpty())
                return QString();

            const int rowIdx =
                d->rowsIndexesToShow ? d->rowsIndexesToShow->at(index.row()) : index.row();
            const auto& entry = d->data->rawData().entries[rowIdx];
            const auto iter = entry.find(key);
            if (iter != entry.cend())
                return iter->second;
            return QString();
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
    return d->data.data();
}

void LookupListEntriesModel::setListModel(LookupListModel* value)
{
    beginResetModel();
    d->data = value;
    endResetModel();

    emit listModelChanged();

    if (d->data)
    {
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

    if (!NX_ASSERT(!rows.empty()))
        return;

    for (auto& index: rows | ranges::views::reverse)
    {
        beginRemoveRows({}, index, index);
        d->data->rawData().entries.erase(d->data->rawData().entries.begin() + index);
        endRemoveRows();
        emit rowCountChanged();
    }
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

    QnTableExportHelper::exportToStreamCsv(this, indexes, outputCsv);
}

bool LookupListEntriesModel::updateHeaders(const QStringList& headers)
{
     if (!NX_ASSERT(d->data))
        return false;

    auto currentHeaders = d->data->attributeNames();
    bool differenceFound = false;
    int i = 0;

    // Replacing old headers. If data is not empty we should update entries as well.
    for(; i < currentHeaders.size(); ++i)
    {
        if (headers.size() <= i)
            break;

        if (headers[i] != currentHeaders[i])
        {
            currentHeaders[i] = headers[i];
            differenceFound = true;
        }
    }
    // If headers is longer then current, append rest of headers.
    for (; i < headers.size(); ++i)
    {
        currentHeaders.append(headers[i]);
        differenceFound = true;
    }

    if (differenceFound)
    {
        d->data->setAttributeNames(currentHeaders);
        // TODO: @pprivalov Update data if nessessary. Do it when validate will be implemented.
    }
    return true;
}

// TODO Implement it later.
bool LookupListEntriesModel::validate()
{
    /*QStringList unparsed;
    for (const auto& entry : m_data->rawData().entries)
    {
        for(const auto& iter : entry)
        {
            QString typeId = m_data->rawData().objectTypeId;
            // TODO validate if iter.second matches the restrictions of iter.first
        }

    }*/
    return true; //unparsed.empty();
}

void LookupListEntriesModel::setFilter(const QString& searchText, int resultLimit)
{
    if (!NX_ASSERT(d->data))
        return;

    beginResetModel();
    if (searchText.isEmpty())
        d->rowsIndexesToShow.reset();
    else
        d->rowsIndexesToShow = d->data->setFilter(searchText, resultLimit);
    endResetModel();
}

} // namespace nx::vms::client::desktop
