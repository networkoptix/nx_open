// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "lookup_list_preview_entries_model.h"

#include <QtCore/QString>

#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <ui/dialogs/common/custom_file_dialog.h>

#include "lookup_list_model.h"

namespace {

const QString kDoNotImportText = "Do not import";

} // namespace

namespace nx::vms::client::desktop {

struct LookupListPreviewEntriesModel::Private
{
    QList<QList<QString>> columnHeaders;
    QSet<QString> attributeNames;
    QList<QString> defaultColumnHeader;
    QMap<int, QString> columnIndexToAttribute;
    PreviewRawData data;
    LookupListEntriesModel* listEntriesModel = nullptr;
    void initColumnHeaders(const QList<QString>& listModelAttributesNames);
    void reset();
};

void LookupListPreviewEntriesModel::Private::reset()
{
    data.clear();
    columnHeaders.clear();
    defaultColumnHeader.clear();
    columnIndexToAttribute.clear();
    attributeNames.clear();
}

void LookupListPreviewEntriesModel::Private::initColumnHeaders(
    const QList<QString>& listModelAttributesNames)
{
    reset();

    // Set the column header as string "Do not import" + list of all attribute names.
    defaultColumnHeader.push_back(kDoNotImportText);
    defaultColumnHeader += listModelAttributesNames;
    attributeNames =
        QSet<QString>(listModelAttributesNames.begin(), listModelAttributesNames.end());

    // "Do not import" is by index 0, so skip it.
    for (int attributeNameIndex = 1; attributeNameIndex < defaultColumnHeader.size();
         attributeNameIndex++)
    {
        QList<QString> curColumnHeader = defaultColumnHeader;
        // Set current attribute name as first choice for current column.
        curColumnHeader.move(curColumnHeader.indexOf(defaultColumnHeader[attributeNameIndex]), 0);
        columnHeaders.push_back(curColumnHeader);
    }

    // Set default association of column index to attribute name.
    for (int attributeNamesIndex = 0; attributeNamesIndex < listModelAttributesNames.size();
        ++attributeNamesIndex)
    {
        columnIndexToAttribute[attributeNamesIndex] =
            listModelAttributesNames[attributeNamesIndex];
    }
}

LookupListPreviewEntriesModel::LookupListPreviewEntriesModel(QAbstractTableModel* parent):
    base_type(parent),
    d(new Private())
{
}

LookupListPreviewEntriesModel::~LookupListPreviewEntriesModel()
{
}

QVariant LookupListPreviewEntriesModel::headerData(
    int section, Qt::Orientation orientation, int role) const
{
    if (!NX_ASSERT(orientation == Qt::Orientation::Horizontal))
        return {};

    if (role != Qt::DisplayRole || section >= d->columnHeaders.size())
        return {};

    return d->columnHeaders[section];
}

QMap<int, QString> LookupListPreviewEntriesModel::columnIndexToAttribute()
{
    return d->columnIndexToAttribute;
}

void LookupListPreviewEntriesModel::headerIndexChanged(int index, const QString& headerName)
{
    if (!d->attributeNames.contains(headerName))
    {
        // "Do not import" or unknown attribute was chosen.
        d->columnIndexToAttribute.remove(index);
    }
    else
    {
        d->columnIndexToAttribute[index] = headerName;
    }
}

void LookupListPreviewEntriesModel::reset()
{
    beginResetModel();
    d->reset();
    endResetModel();
    emit rowCountChanged();
}

int LookupListPreviewEntriesModel::rowCount(const QModelIndex& /*parent*/) const
{
    return d->data.size();
}

int LookupListPreviewEntriesModel::columnCount(const QModelIndex& /*parent*/) const
{
    return d->data.size() ? d->data.front().size() : 0;
}

QString LookupListPreviewEntriesModel::doNotImportText() const
{
    return kDoNotImportText;
}

QVariant LookupListPreviewEntriesModel::data(const QModelIndex& index, int role) const
{
    if (role != Qt::DisplayRole || !hasIndex(index.row(), index.column()))
        return {};

    return d->data[index.row()][index.column()];
}

void LookupListPreviewEntriesModel::setRawData(const PreviewRawData& rawData)
{
    if (!NX_ASSERT(d->listEntriesModel))
        return;

    beginResetModel();
    d->initColumnHeaders(d->listEntriesModel->listModel()->attributeNames());
    d->data = rawData;

    if (d->data.size() && d->data.front().size() != d->columnHeaders.size())
        d->columnHeaders.resize(d->data.front().size(), d->defaultColumnHeader);

    endResetModel();
    emit rowCountChanged();
}

LookupListEntriesModel* LookupListPreviewEntriesModel::lookupListEntriesModel()
{
    return d->listEntriesModel;
}

void LookupListPreviewEntriesModel::setLookupListEntriesModel(
    LookupListEntriesModel* lookupListEntriesModel)
{
    if (lookupListEntriesModel == d->listEntriesModel)
        return;

    if (d->listEntriesModel)
        disconnect(d->listEntriesModel);

    beginResetModel();
    reset();

    d->listEntriesModel = lookupListEntriesModel;

    if (d->listEntriesModel)
    {
        connect(d->listEntriesModel,
            &LookupListEntriesModel::listModelChanged,
            [this](LookupListModel* listModel)
            {
                if (listModel)
                {
                    d->initColumnHeaders(listModel->attributeNames());
                    emit headerDataChanged(Qt::Horizontal, 0, d->columnHeaders.size());
                }
            });
        emit lookupListEntriesModelChanged();
    }
    endResetModel();
}
} // namespace nx::vms::client::desktop
