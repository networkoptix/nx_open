// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "lookup_list_preview_processor.h"

#include <QtCore/QString>

#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <ui/dialogs/common/custom_file_dialog.h>

#include "lookup_list_model.h"

namespace nx::vms::client::desktop {

LookupListPreviewHelper* LookupListPreviewHelper::instance()
{
    static LookupListPreviewHelper instance;
    return &instance;
}

struct LookupListPreviewProcessor::Private
{
    int rowsNumber = 10;
    QString separator;
    QString filePath;
    bool dataHasHeaderRow = false;
};

LookupListPreviewProcessor::LookupListPreviewProcessor(QObject* parent):
    base_type(parent),
    d(new Private())
{
}

LookupListPreviewHelper::LookupListPreviewHelper(QObject* parent):
    base_type(parent)
{
}

QString LookupListPreviewHelper::getImportFilePathFromDialog()
{
    QString previousDir = appContext()->localSettings()->lastImportDir();
    if (previousDir.isEmpty() && !appContext()->localSettings()->mediaFolders().isEmpty())
        previousDir = appContext()->localSettings()->mediaFolders().first();

    const auto options = QnCustomFileDialog::fileDialogOptions();
    const QString caption = tr("Import Lookup List");
    const QString fileFilters = QnCustomFileDialog::createFilter(
        {{tr("Text files"), {"txt", "csv", "tsv"}}, {tr("All other text files"), {"*"}}});
    return QFileDialog::getOpenFileName(nullptr, caption, previousDir, fileFilters, nullptr, options);
}

bool LookupListPreviewProcessor::setImportFilePathFromDialog()
{
    const auto fileName = LookupListPreviewHelper::getImportFilePathFromDialog();

    if (fileName.isEmpty())
        return false;

    setDataHasHeaderRow(true);
    setFilePath(fileName);

    if (d->separator.isEmpty())
    {
        const auto extension = QFileInfo(fileName).suffix();
        // By default set separator as comma.
        setSeparator(extension == "tsv" ? "\t" : ",");
    }

    appContext()->localSettings()->lastImportDir = QFileInfo(fileName).absolutePath();
    return true;
}

bool LookupListPreviewProcessor::buildTablePreview(LookupListImportEntriesModel* model,
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

        LookupListImportEntriesModel::PreviewRawData newData;
        while (!in.atEnd() && lineIndex <= d->rowsNumber)
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

void LookupListPreviewProcessor::reset(LookupListImportEntriesModel* model)
{
    if (!NX_ASSERT(model))
        return;

    setFilePath({});
    setSeparator({});
    setDataHasHeaderRow(false);
    model->reset();
}

} // namespace nx::vms::client::desktop
