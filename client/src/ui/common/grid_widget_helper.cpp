#include "grid_widget_helper.h"

#include <QtCore/QMimeData>
#include <QtGui/QClipboard>
#include <QtWidgets/QTableView>
#include <QtWidgets/QApplication>

#include <client/client_settings.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/file_dialog.h>

#include <utils/common/string.h>

void QnGridWidgetHelper::exportToFile(QTableView *grid, QWidget *parent, const QString &caption)
{
    QString previousDir = qnSettings->lastExportDir();
    if (previousDir.isEmpty())
        previousDir = qnSettings->mediaFolder();
    QString fileName;
    while (true) {
        QScopedPointer<QnCustomFileDialog> dialog(new QnCustomFileDialog(
                                                      parent,
                                                      caption,
                                                      previousDir,
                                                      tr("HTML file (*.html);;Spread Sheet (CSV) File(*.csv)")));
        dialog->setAcceptMode(QFileDialog::AcceptSave);
        if (dialog->exec() != QDialog::Accepted)
            return;

        fileName = dialog->selectedFile();
        if (fileName.isEmpty())
            return;

        QString selectedExtension = dialog->selectedExtension();
        if (!fileName.toLower().endsWith(selectedExtension)) {
            fileName += selectedExtension;

            if (QFile::exists(fileName)) {
                QDialogButtonBox::StandardButton button = QnMessageBox::information(
                    parent,
                    tr("Save As"),
                    tr("File '%1' already exists. Overwrite?").arg(QFileInfo(fileName).completeBaseName()),
                    QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel
                );

                if(button == QDialogButtonBox::Cancel || button == QDialogButtonBox::No)
                    return;
            }
        }

        if (QFile::exists(fileName) && !QFile::remove(fileName)) {
            QnMessageBox::critical(
                parent,
                tr("Could not overwrite file"),
                tr("File '%1' is used by another process. Please try another name.").arg(QFileInfo(fileName).completeBaseName()),
                QDialogButtonBox::Ok
            );
            continue;
        }

        break;
    }
    qnSettings->setLastExportDir(QFileInfo(fileName).absolutePath());


    QString textData;
    QString htmlData;
    getGridData(grid, QLatin1Char(L';'), &textData, &htmlData);

    QFile f(fileName);
    if (f.open(QFile::WriteOnly))
    {
        if (fileName.endsWith(lit(".html")) || fileName.endsWith(lit(".htm")))
            f.write(htmlData.toUtf8());
        else
            f.write(textData.toUtf8());
    }
}

void QnGridWidgetHelper::copyToClipboard(QTableView* grid)
{
    QString textData;
    QString htmlData;
    getGridData(grid, QLatin1Char(L'\t'), &textData, &htmlData);

    QMimeData* mimeData = new QMimeData();
    mimeData->setText(textData);
    mimeData->setHtml(htmlData);
    QApplication::clipboard()->setMimeData(mimeData);
}

void QnGridWidgetHelper::getGridData(QTableView *grid, const QLatin1Char &textDelimiter, QString *textData, QString *htmlData)
{
    QAbstractItemModel *model = grid->model();
    QModelIndexList list = grid->selectionModel()->selectedIndexes();
    if(list.isEmpty())
        return;

    qSort(list);

    QString textResult, htmlResult;

    htmlResult.append(lit("<html>\n"));
    htmlResult.append(lit("<body>\n"));
    htmlResult.append(lit("<table>\n"));

    htmlResult.append(lit("<tr>"));
    for(int i = 0; i < list.size() && list[i].row() == list[0].row(); ++i)
    {
        if (i > 0)
            textResult.append(textDelimiter);
        QString header = model->headerData(list[i].column(), Qt::Horizontal).toString();
        htmlResult.append(lit("<th>"));
        htmlResult.append(header);
        htmlResult.append(lit("</th>"));
        textResult.append(header);
    }
    htmlResult.append(lit("</tr>"));

    int prevRow = -1;
    for(int i = 0; i < list.size(); ++i)
    {
        if(list[i].row() != prevRow) {
            prevRow = list[i].row();
            textResult.append(lit("\n"));
            if (i > 0)
                htmlResult.append(lit("</tr>"));
            htmlResult.append(lit("<tr>"));
        }
        else {
            textResult.append(textDelimiter);
        }

        htmlResult.append(lit("<td>"));
        htmlResult.append(model->data(list[i], Qn::DisplayHtmlRole).toString());
        htmlResult.append(lit("</td>"));

        textResult.append(model->data(list[i]).toString());
    }
    htmlResult.append(lit("</tr>\n"));
    htmlResult.append(lit("</table>\n"));
    htmlResult.append(lit("</body>\n"));
    htmlResult.append(lit("</html>\n"));
    textResult.append(lit("\n"));

    *textData = textResult;
    *htmlData = htmlResult;
}
