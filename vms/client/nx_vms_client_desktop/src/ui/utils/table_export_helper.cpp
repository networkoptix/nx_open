// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "table_export_helper.h"

#include <QtCore/QMimeData>
#include <QtGui/QClipboard>
#include <QtWidgets/QApplication>

#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/common/html/html.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/file_messages.h>
#include <ui/dialogs/common/message_box.h>

using namespace nx::vms::client::desktop;

QString QnTableExportHelper::getExportFilePathFromDialog(
    const QString& fileFilters,
    QWidget* parent,
    const QString& caption)
{
    QString previousDir = appContext()->localSettings()->lastExportDir();
    if (previousDir.isEmpty() && !appContext()->localSettings()->mediaFolders().isEmpty())
        previousDir = appContext()->localSettings()->mediaFolders().first();
    QString fileName;
    while (true)
    {
        QScopedPointer<QnCustomFileDialog> dialog(new QnCustomFileDialog(
            parent,
            caption,
            previousDir,
            fileFilters));
        dialog->setAcceptMode(QFileDialog::AcceptSave);
        if (dialog->exec() != QDialog::Accepted)
            return {};

        fileName = dialog->selectedFile();
        if (fileName.isEmpty())
            return {};

        QString selectedExtension = dialog->selectedExtension();
        if (!fileName.toLower().endsWith(selectedExtension))
        {
            fileName += selectedExtension;

            if (QFile::exists(fileName)
                && !QnFileMessages::confirmOverwrite(
                    parent, QFileInfo(fileName).completeBaseName()))
            {
                return {};
            }
        }

        if (QFile::exists(fileName) && !QFile::remove(fileName))
        {
            const auto extras = QFileInfo(fileName).completeBaseName() + '\n'
                + tr("Close all programs which may use this file and try again");

            QnMessageBox::warning(parent, tr("File used by another process"), extras);
            continue;
        }

        break;
    }

    appContext()->localSettings()->lastExportDir = QFileInfo(fileName).absolutePath();
    return fileName;
}

void QnTableExportHelper::exportToFile(
    const QAbstractItemModel* tableModel,
    const QModelIndexList& indexes,
    QWidget* parent,
    const QString& caption,
    const int dataRole)
{
    const QString filters = QnCustomFileDialog::createFilter(
        {{tr("HTML file"), {"html"}}, {tr("Spread Sheet (CSV) File"), {"csv"}}});

    const QString fileName =
        QnTableExportHelper::getExportFilePathFromDialog(filters, parent, caption);
    if (fileName.isEmpty())
        return;

    QFile file(fileName);
    if (file.open(QFile::WriteOnly | QFile::Truncate))
    {
        if (fileName.endsWith(".html") || fileName.endsWith(".htm"))
            file.write(getGridHtmlData(tableModel, indexes, dataRole).toUtf8());
        else
            file.write(getGridCsvData(tableModel, indexes, ';', dataRole).toUtf8());
    }
}

void QnTableExportHelper::exportToStreamCsv(const QAbstractItemModel* tableModel,
    const QModelIndexList& indexes,
    QTextStream& outputCsv,
    int dataRole)
{
    outputCsv << getGridCsvData(tableModel, indexes, ',', dataRole);
}

void QnTableExportHelper::copyToClipboard(
    const QAbstractItemModel* tableModel,
    const QModelIndexList& indexes,
    int dataRole)
{
    QString textData;
    QString htmlData;
    getGridHtmlCsvData(tableModel, indexes, '\t', textData, htmlData, dataRole);

    QMimeData* mimeData = new QMimeData();
    mimeData->setText(textData);
    mimeData->setHtml(htmlData);
    QApplication::clipboard()->setMimeData(mimeData);
}

QString QnTableExportHelper::getGridCsvData(const QAbstractItemModel* tableModel,
    const QModelIndexList& indexes,
    const QChar& textDelimiter,
    int dataRole)
{
    QString textResult;

    using namespace nx::vms::common;

    QModelIndexList sortedIndexes = indexes;
    std::sort(sortedIndexes.begin(), sortedIndexes.end());

    /* Creating header. */
    for (int i = 0; i < sortedIndexes.size() && sortedIndexes[i].row() == sortedIndexes[0].row();
         ++i)
    {
        int column = sortedIndexes[i].column();

        QString header = tableModel->headerData(column, Qt::Horizontal).toString();

        if (i > 0)
            textResult.append(textDelimiter);

        textResult.append(header);
    }

    /* Fill table with data */

    int prevRow = -1;
    for (int i = 0; i < sortedIndexes.size(); ++i)
    {
        if (sortedIndexes[i].row() != prevRow)
        {
            prevRow = sortedIndexes[i].row();
            textResult.append("\n");
        }
        else
        {
            textResult.append(textDelimiter);
        }

        QString textData = tableModel->data(sortedIndexes[i], dataRole).toString();
        textResult.append(textData);
    }

    textResult.append("\n");

    return textResult;
}

QString QnTableExportHelper::getGridHtmlData(const QAbstractItemModel* tableModel,
    const QModelIndexList& indexes,
    int dataRole)
{
    using namespace nx::vms::common;

    QModelIndexList sortedIndexes = indexes;
    std::sort(sortedIndexes.begin(), sortedIndexes.end());

    QString htmlResult;
    {
        html::Tag htmlTag("html", htmlResult, html::Tag::BothBreaks);
        html::Tag bodyTag("body", htmlResult, html::Tag::BothBreaks);
        html::Tag tableTag("table", htmlResult, html::Tag::BothBreaks);

        { /* Creating header. */
            html::Tag rowGuard("tr", htmlResult);
            for (int i = 0;
                 i < sortedIndexes.size() && sortedIndexes[i].row() == sortedIndexes[0].row();
                 ++i)
            {
                int column = sortedIndexes[i].column();

                QString header = tableModel->headerData(column, Qt::Horizontal).toString();
                html::Tag thTag("th", htmlResult, html::Tag::NoBreaks);
                htmlResult.append(header);
            }
        }

        { /* Fill table with data */

            QScopedPointer<html::Tag> rowTag;

            int prevRow = -1;
            for (int i = 0; i < sortedIndexes.size(); ++i)
            {
                if (sortedIndexes[i].row() != prevRow)
                {
                    prevRow = sortedIndexes[i].row();
                    rowTag.reset(); /*< close tag before opening new. */
                    rowTag.reset(new html::Tag("tr", htmlResult));
                }

                QString textData = tableModel->data(sortedIndexes[i], dataRole).toString();

                QString dataString =
                    tableModel->data(sortedIndexes[i], Qn::DisplayHtmlRole).toString();
                if (dataString.isEmpty())
                    dataString = textData.toHtmlEscaped();

                html::Tag cellTag("td", htmlResult, html::Tag::NoBreaks);
                htmlResult.append(dataString);
            }
        }
    }
    return htmlResult;
}

void QnTableExportHelper::getGridHtmlCsvData(const QAbstractItemModel* tableModel,
    const QModelIndexList& indexes,
    const QChar& textDelimiter,
    QString& textData,
    QString& htmlData,
    int dataRole)
{
    using namespace nx::vms::common;

    QModelIndexList sortedIndexes = indexes;
    std::sort(sortedIndexes.begin(), sortedIndexes.end());

    QString textResult;
    QString htmlResult;
    {
        html::Tag htmlTag("html", htmlResult, html::Tag::BothBreaks);
        html::Tag bodyTag("body", htmlResult, html::Tag::BothBreaks);
        html::Tag tableTag("table", htmlResult, html::Tag::BothBreaks);

        { /* Creating header. */
            html::Tag rowGuard("tr", htmlResult);
            for (int i = 0;
                i < sortedIndexes.size() && sortedIndexes[i].row() == sortedIndexes[0].row(); ++i)
            {
                int column = sortedIndexes[i].column();

                QString header = tableModel->headerData(column, Qt::Horizontal).toString();
                html::Tag thTag("th", htmlResult, html::Tag::NoBreaks);
                htmlResult.append(header);

                if (i > 0)
                    textResult.append(textDelimiter);
                textResult.append(header);
            }
        }

        { /* Fill table with data */

            QScopedPointer<html::Tag> rowTag;

            int prevRow = -1;
            for (int i = 0; i < sortedIndexes.size(); ++i)
            {
                if (sortedIndexes[i].row() != prevRow)
                {
                    prevRow = sortedIndexes[i].row();
                    textResult.append("\n");
                    rowTag.reset();   /*< close tag before opening new. */
                    rowTag.reset(new html::Tag("tr", htmlResult));
                }
                else
                {
                    textResult.append(textDelimiter);
                }

                QString textData = tableModel->data(sortedIndexes[i], dataRole).toString();
                textResult.append(textData);

                QString dataString =
                    tableModel->data(sortedIndexes[i], Qn::DisplayHtmlRole).toString();
                if (dataString.isEmpty())
                    dataString = textData.toHtmlEscaped();

                html::Tag cellTag("td", htmlResult, html::Tag::NoBreaks);
                htmlResult.append(dataString);
            }
        }

        textResult.append("\n");
    }

    textData = textResult;
    htmlData = htmlResult;
}

QModelIndexList QnTableExportHelper::getFilteredIndexes(
    QAbstractItemModel* model, FilterFunction filter)
{
    QModelIndexList result;
    for (int row = 0; row < model->rowCount(); ++row)
    {
        for (int col = 0; col < model->columnCount(); ++col)
        {
            if (model->hasIndex(row, col) && filter(model->index(row, col)))
                result << model->index(row, col);
        }
    }
    return result;
}

QModelIndexList QnTableExportHelper::getAllIndexes(QAbstractItemModel* model)
{
    return QnTableExportHelper::getFilteredIndexes(
        model, [&](const QModelIndex&) { return true; });
}

QnTableExportCompositeModel::QnTableExportCompositeModel(
    QList<QAbstractItemModel*> models,
    QObject* parent)
    :
    QAbstractItemModel(parent),
    m_models(models)
{
}

int QnTableExportCompositeModel::rowCount(const QModelIndex& parent) const
{
    if (m_models.isEmpty() || parent.isValid())
        return 0;
    return std::accumulate(m_models.cbegin(), m_models.cend(), 0,
        [](int sum, QAbstractItemModel* model) { return sum + model->rowCount(QModelIndex()); });
}

int QnTableExportCompositeModel::columnCount(const QModelIndex& parent) const
{
    if (m_models.isEmpty() || parent.isValid())
        return 0;
    return m_models.first()->columnCount(QModelIndex());
}

QVariant QnTableExportCompositeModel::data(const QModelIndex& index, int role) const
{
    const auto subTableRow = calculateSubTableRow(index.row());
    const auto subModel = m_models.at(subTableRow.first);
    if (subModel->columnCount() > index.column())
        return subModel->data(subModel->index(subTableRow.second, index.column(), QModelIndex()));
    return QVariant();
}

QVariant QnTableExportCompositeModel::headerData(
    int section,
    Qt::Orientation orientation,
    int role) const
{
    if (orientation == Qt::Horizontal)
    {
        return m_models.first()->headerData(section, orientation, role);
    }
    else
    {
        auto subTableRow = calculateSubTableRow(section);
        const auto subModel = m_models.at(subTableRow.first);
        return subModel->headerData(subTableRow.second, orientation, role);
    }
}

QModelIndex QnTableExportCompositeModel::index(
    int row,
    int column,
    const QModelIndex& parent) const
{
    return createIndex(row, column);
}

QModelIndex QnTableExportCompositeModel::parent(const QModelIndex& index) const
{
    return QModelIndex();
}

std::pair<int, int> QnTableExportCompositeModel::calculateSubTableRow(int row) const
{
    int subTableIndex = 0;
    int subTableRow = row;
    while (subTableRow >= m_models.at(subTableIndex)->rowCount(QModelIndex()))
    {
        subTableRow -= m_models.at(subTableIndex)->rowCount(QModelIndex());
        subTableIndex++;
    }
    return std::make_pair(subTableIndex, subTableRow);
}
