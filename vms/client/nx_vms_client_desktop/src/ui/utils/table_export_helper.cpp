#include "table_export_helper.h"

#include <QtCore/QMimeData>
#include <QtWidgets/QApplication>
#include <QtGui/QClipboard>

#include <client/client_settings.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/file_messages.h>
#include <ui/dialogs/common/message_box.h>

#include <utils/common/html.h>

void QnTableExportHelper::exportToFile(
    const QAbstractItemModel* tableModel,
    const QModelIndexList& indexes,
    QWidget* parent,
    const QString& caption)
{
    QString previousDir = qnSettings->lastExportDir();
    if (previousDir.isEmpty())
        previousDir = qnSettings->mediaFolder();
    QString fileName;
    while (true)
    {
        const QString filters = QnCustomFileDialog::createFilter(
            {
                {tr("HTML file"), {"html"}},
                {tr("Spread Sheet (CSV) File"), {"csv"}}
            });

        QScopedPointer<QnCustomFileDialog> dialog(new QnCustomFileDialog(
            parent,
            caption,
            previousDir,
            filters));
        dialog->setAcceptMode(QFileDialog::AcceptSave);
        if (dialog->exec() != QDialog::Accepted)
            return;

        fileName = dialog->selectedFile();
        if (fileName.isEmpty())
            return;

        QString selectedExtension = dialog->selectedExtension();
        if (!fileName.toLower().endsWith(selectedExtension))
        {
            fileName += selectedExtension;

            if (QFile::exists(fileName)
                && !QnFileMessages::confirmOverwrite(
                    parent, QFileInfo(fileName).completeBaseName()))
            {
                return;
            }
        }

        if (QFile::exists(fileName) && !QFile::remove(fileName))
        {
            const auto extras = QFileInfo(fileName).completeBaseName() + L'\n'
                + tr("Close all programs which may use this file and try again");

            QnMessageBox::warning(parent, tr("File used by another process"), extras);
            continue;
        }

        break;
    }
    qnSettings->setLastExportDir(QFileInfo(fileName).absolutePath());

    QString textData;
    QString htmlData;
    getGridData(tableModel, indexes, L';', textData, htmlData);

    QFile file(fileName);
    if (file.open(QFile::WriteOnly | QFile::Truncate))
    {
        if (fileName.endsWith(lit(".html")) || fileName.endsWith(lit(".htm")))
            file.write(htmlData.toUtf8());
        else
            file.write(textData.toUtf8());
    }
}

void QnTableExportHelper::copyToClipboard(
    const QAbstractItemModel* tableModel,
    const QModelIndexList& indexes)
{
    QString textData;
    QString htmlData;
    getGridData(tableModel, indexes, L'\t', textData, htmlData);

    QMimeData* mimeData = new QMimeData();
    mimeData->setText(textData);
    mimeData->setHtml(htmlData);
    QApplication::clipboard()->setMimeData(mimeData);
}

void QnTableExportHelper::getGridData(
    const QAbstractItemModel* tableModel,
    const QModelIndexList& indexes,
    const QChar& textDelimiter,
    QString& textData,
    QString& htmlData)
{
    QModelIndexList sortedIndexes = indexes;
    std::sort(sortedIndexes.begin(), sortedIndexes.end());

    QString textResult;
    QString htmlResult;
    {
        QnHtmlTag htmlTag("html", htmlResult, QnHtmlTag::BothBreaks);
        QnHtmlTag bodyTag("body", htmlResult, QnHtmlTag::BothBreaks);
        QnHtmlTag tableTag("table", htmlResult, QnHtmlTag::BothBreaks);

        { /* Creating header. */
            QnHtmlTag rowGuard("tr", htmlResult);
            for (int i = 0;
                i < sortedIndexes.size() && sortedIndexes[i].row() == sortedIndexes[0].row(); ++i)
            {
                int column = sortedIndexes[i].column();

                QString header = tableModel->headerData(column, Qt::Horizontal).toString();
                QnHtmlTag thTag("th", htmlResult, QnHtmlTag::NoBreaks);
                htmlResult.append(header);

                if (i > 0)
                    textResult.append(textDelimiter);
                textResult.append(header);
            }
        }

        { /* Fill table with data */

            QScopedPointer<QnHtmlTag> rowTag;

            int prevRow = -1;
            for (int i = 0; i < sortedIndexes.size(); ++i)
            {
                if (sortedIndexes[i].row() != prevRow)
                {
                    prevRow = sortedIndexes[i].row();
                    textResult.append(lit("\n"));
                    rowTag.reset();   /*< close tag before opening new. */
                    rowTag.reset(new QnHtmlTag("tr", htmlResult));
                }
                else
                {
                    textResult.append(textDelimiter);
                }

                QString textData = tableModel->data(sortedIndexes[i], Qt::DisplayRole).toString();
                textResult.append(textData);

                QString dataString =
                    tableModel->data(sortedIndexes[i], Qn::DisplayHtmlRole).toString();
                if (dataString.isEmpty())
                    dataString = escapeHtml(textData);

                QnHtmlTag cellTag("td", htmlResult, QnHtmlTag::NoBreaks);
                htmlResult.append(dataString);
            }
        }

        textResult.append(lit("\n"));
    }

    textData = textResult;
    htmlData = htmlResult;
}

QModelIndexList QnTableExportHelper::getAllIndexes(QAbstractItemModel* model)
{
    QModelIndexList result;
    for (int row = 0; row < model->rowCount(); ++row)
    {
        for (int col = 0; col < model->columnCount(); ++col)
        {
            if (model->hasIndex(row, col))
                result << model->index(row, col);
        }
    }
    return result;
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
