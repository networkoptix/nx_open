#include "table_export_helper.h"

#include <QtCore/QMimeData>
#include <QtCore/QAbstractItemModel>

#include <QtGui/QClipboard>

#include <QtWidgets/QAbstractItemView>
#include <QtWidgets/QApplication>

#include <client/client_settings.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/file_dialog.h>
#include <ui/dialogs/common/file_messages.h>

#include <utils/common/html.h>

void QnTableExportHelper::exportToFile(QAbstractItemView* grid, bool onlySelected, QWidget* parent, const QString& caption)
{
    QString previousDir = qnSettings->lastExportDir();
    if (previousDir.isEmpty())
        previousDir = qnSettings->mediaFolder();
    QString fileName;
    while (true)
    {
        QScopedPointer<QnCustomFileDialog> dialog(new QnCustomFileDialog(
            parent,
            caption,
            previousDir,
            tr("HTML file (*.html);;Spread Sheet (CSV) File (*.csv)")));
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
    getGridData(grid, onlySelected, L';', &textData, &htmlData);

    QFile f(fileName);
    if (f.open(QFile::WriteOnly))
    {
        if (fileName.endsWith(lit(".html")) || fileName.endsWith(lit(".htm")))
            f.write(htmlData.toUtf8());
        else
            f.write(textData.toUtf8());
    }
}

void QnTableExportHelper::copyToClipboard(QAbstractItemView* grid, bool onlySelected)
{
    QString textData;
    QString htmlData;
    getGridData(grid, onlySelected, L'\t', &textData, &htmlData);

    QMimeData* mimeData = new QMimeData();
    mimeData->setText(textData);
    mimeData->setHtml(htmlData);
    QApplication::clipboard()->setMimeData(mimeData);
}

void QnTableExportHelper::getGridData(QAbstractItemView* grid, bool onlySelected, const QChar& textDelimiter, QString* textData, QString* htmlData)
{
    QAbstractItemModel *model = grid->model();
    QModelIndexList list = onlySelected
        ? grid->selectionModel()->selectedIndexes()
        : getAllIndexes(model);

    std::sort(list.begin(), list.end());

    QString textResult;
    QString htmlResult;
    {
        QnHtmlTag htmlTag("html", htmlResult, QnHtmlTag::BothBreaks);
        QnHtmlTag bodyTag("body", htmlResult, QnHtmlTag::BothBreaks);
        QnHtmlTag tableTag("table", htmlResult, QnHtmlTag::BothBreaks);

        { /* Creating header. */
            QnHtmlTag rowGuard("tr", htmlResult);
            for (int i = 0; i < list.size() && list[i].row() == list[0].row(); ++i)
            {
                int column = list[i].column();

                QString header = model->headerData(column, Qt::Horizontal).toString();
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
            for (int i = 0; i < list.size(); ++i)
            {
                if (list[i].row() != prevRow)
                {
                    prevRow = list[i].row();
                    textResult.append(lit("\n"));
                    rowTag.reset();   /*< close tag before opening new. */
                    rowTag.reset(new QnHtmlTag("tr", htmlResult));
                }
                else
                {
                    textResult.append(textDelimiter);
                }

                QString textData = model->data(list[i], Qt::DisplayRole).toString();
                textResult.append(textData);

                QString htmlData = model->data(list[i], Qn::DisplayHtmlRole).toString();
                if (htmlData.isEmpty())
                    htmlData = escapeHtml(textData);

                QnHtmlTag cellTag("td", htmlResult, QnHtmlTag::NoBreaks);
                htmlResult.append(htmlData);
            }
        }

        textResult.append(lit("\n"));
    }

    *textData = textResult;
    *htmlData = htmlResult;
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
