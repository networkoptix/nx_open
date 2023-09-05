// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "lookup_list_preview_processor.h"

#include <QtCore/QString>

#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <ui/dialogs/common/custom_file_dialog.h>

#include "lookup_list_model.h"

namespace nx::vms::client::desktop {

struct LookupListPreviewProcessor::Private
{
    int rowsNumber;
    QString separator;
    QString filePath;
    bool dataHasHeaderRow;
};

struct LookupPreviewEntriesModel::Private
{
    QList<QString> columnHeaders;
    PreviewRawData data;
};

LookupPreviewEntriesModel::LookupPreviewEntriesModel(QAbstractTableModel* parent):
    base_type(parent),
    d(new Private())
{
}

LookupPreviewEntriesModel::~LookupPreviewEntriesModel()
{
}

QVariant LookupPreviewEntriesModel::headerData(
    int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Orientation::Horizontal || role != Qt::DisplayRole
        || section >= d->columnHeaders.size())
    {
        return {};
    }

    return d->columnHeaders[section];
}

void LookupPreviewEntriesModel::setHeaderRowData(LookupListEntriesModel* model)
{
    if (!NX_ASSERT(model) || !NX_ASSERT(model->listModel()))
        return;

    d->columnHeaders = model->listModel()->attributeNames();
}

void LookupPreviewEntriesModel::reset()
{
    beginResetModel();
    d->columnHeaders.clear();
    d->data.clear();
    endResetModel();
}

int LookupPreviewEntriesModel::rowCount(const QModelIndex& /*parent*/) const
{
    return d->data.size();
}

int LookupPreviewEntriesModel::columnCount(const QModelIndex& /*parent*/) const
{
    return d->data.size() ? d->data.front().size() : 0;
}

QVariant LookupPreviewEntriesModel::data(const QModelIndex& index, int role) const
{
    if (role != Qt::DisplayRole || !hasIndex(index.row(), index.column()))
        return {};

    return d->data[index.row()][index.column()];
}

void LookupPreviewEntriesModel::setRawData(const PreviewRawData& rawData)
{
    emit beginResetModel();
    d->data = rawData;
    emit endResetModel();
}

LookupListPreviewProcessor::LookupListPreviewProcessor(QObject* parent):
    base_type(parent),
    d(new Private())
{
}

void LookupListPreviewProcessor::setImportFilePathFromDialog()
{
    QString previousDir = appContext()->localSettings()->lastImportDir();
    if (previousDir.isEmpty() && !appContext()->localSettings()->mediaFolders().isEmpty())
        previousDir = appContext()->localSettings()->mediaFolders().first();

    const auto options = QnCustomFileDialog::fileDialogOptions();
    const QString caption = tr("Import Lookup List");
    const QString filter = QnCustomFileDialog::createFilter({{tr("Text files"), {"csv","txt","tsv"}}});
    const QString fileName =
        QFileDialog::getOpenFileName(nullptr, caption, previousDir, filter, nullptr, options);

    setDataHasHeaderRow(true);
    setFilePath(fileName);

    if (d->separator.isEmpty())
    {
        const auto extension = QFileInfo(fileName).suffix();
        setSeparator(extension == "csv" ? "," : "\t");
    }

    appContext()->localSettings()->lastImportDir = QFileInfo(fileName).absolutePath();
}

bool LookupListPreviewProcessor::buildTablePreview(LookupPreviewEntriesModel* model,
    const QString& filePath,
    const QString& separator,
    bool hasHeader)
{
    if (!NX_ASSERT(model))
        return false;

    if (filePath.isEmpty() || separator.isEmpty())
        return false;

    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly))
    {
        int lineIndex = 0;
        QTextStream in(&file);

        LookupPreviewEntriesModel::PreviewRawData newData;
        while (!in.atEnd() && lineIndex < d->rowsNumber)
        {
            QString fileLine = in.readLine();
            if (hasHeader && lineIndex == 0)
            {
                ++lineIndex;
                continue;
            }

            std::vector<QVariant> line;
            for(const auto& value: fileLine.split(separator))
                line.push_back(value);

            newData.push_back(line);
            ++lineIndex;
        }
        model->setRawData(newData);
        return true;
    }
    return false;
}

LookupListPreviewProcessor::~LookupListPreviewProcessor()
{
}

void LookupListPreviewProcessor::setRowsNumber(int rowsNumber)
{
    if (rowsNumber != d->rowsNumber)
    {
        d->rowsNumber = rowsNumber;
        emit rowsNumberChanged(d->rowsNumber);
    }
}

void LookupListPreviewProcessor::setSeparator(const QString& separator)
{
    if (separator != d->separator)
    {
        d->separator = separator;
        emit separatorChanged(d->separator);
    }
}

void LookupListPreviewProcessor::setFilePath(const QString& filePath)
{
    if (filePath != d->filePath)
    {
        d->filePath = filePath;
        emit filePathChanged(filePath);
    }
}

int LookupListPreviewProcessor::rowsNumber()
{
    return d->rowsNumber;
}

QString LookupListPreviewProcessor::separator()
{
    return d->separator;
}

QString LookupListPreviewProcessor::filePath()
{
    return d->filePath;
}

void LookupListPreviewProcessor::setDataHasHeaderRow(bool dataHasHeaderRow)
{
    if (dataHasHeaderRow != d->dataHasHeaderRow)
    {
        d->dataHasHeaderRow = dataHasHeaderRow;
        emit dataHasHeaderRowChanged(dataHasHeaderRow);
    }
}

bool LookupListPreviewProcessor::dataHasHeaderRow()
{
    return d->dataHasHeaderRow;
}

void LookupListPreviewProcessor::reset(LookupPreviewEntriesModel* model)
{
    if (!NX_ASSERT(model))
        return;

    setFilePath({});
    setSeparator({});
    setDataHasHeaderRow(false);
    model->reset();
}

} // namespace nx::vms::client::desktop
