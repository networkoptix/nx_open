#include <QTableView>

#include "grid_widget_helper.h"
#include "client/client_settings.h"
#include "ui/dialogs/custom_file_dialog.h"

QnGridWidgetHelper::QnGridWidgetHelper(QnWorkbenchContext *context):
    QnWorkbenchContextAware(context)
{

}

void QnGridWidgetHelper::exportToFile(QTableView* grid)
{
    QString previousDir = qnSettings->lastExportDir();
    if (previousDir.isEmpty())
        previousDir = qnSettings->mediaFolder();
    QString fileName;
    while (true) 
    {

        QScopedPointer<QnCustomFileDialog> dialog(new QnCustomFileDialog(
            mainWindow(),
            QObject::tr("Export selected events to file"),
            previousDir,
            QObject::tr("HTML file (*.html);;CSV file (*.csv)")
            ));
        dialog->setFileMode(QFileDialog::AnyFile);
        dialog->setAcceptMode(QFileDialog::AcceptSave);

        if (!dialog->exec() || dialog->selectedFiles().isEmpty())
            return;

        fileName = dialog->selectedFiles().value(0);
        QString selectedFilter = dialog->selectedNameFilter();
        QString selectedExtension = selectedFilter.mid(selectedFilter.lastIndexOf(QLatin1Char('.')), 4);

        if (fileName.isEmpty())
            return;

        if (!fileName.toLower().endsWith(selectedExtension)) {
            fileName += selectedExtension;

            if (QFile::exists(fileName)) {
                QMessageBox::StandardButton button = QMessageBox::information(
                    mainWindow(),
                    QObject::tr("Save As"),
                    QObject::tr("File '%1' already exists. Overwrite?").arg(QFileInfo(fileName).baseName()),
                    QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel
                    );

                if(button == QMessageBox::Cancel || button == QMessageBox::No)
                    return;
            }
        }

        if (QFile::exists(fileName) && !QFile::remove(fileName)) {
            QMessageBox::critical(
                mainWindow(),
                QObject::tr("Could not overwrite file"),
                QObject::tr("File '%1' is used by another process. Please try another name.").arg(QFileInfo(fileName).baseName()),
                QMessageBox::Ok
                );
            continue;
        }

        break;
    }
    qnSettings->setLastExportDir(QFileInfo(fileName).absolutePath());


    QString textData;
    QString htmlData;
    getGridData(grid, textData, htmlData, QLatin1Char(L';'));

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
    getGridData(grid, textData, htmlData, QLatin1Char(L'\t'));

    QMimeData* mimeData = new QMimeData();
    mimeData->setText(textData);
    mimeData->setHtml(htmlData);
    QApplication::clipboard()->setMimeData(mimeData);
}

void QnGridWidgetHelper::getGridData(QTableView* grid, QString& textData, QString& htmlData, const QLatin1Char& textDelimiter)
{
    QAbstractItemModel *model = grid->model();
    QModelIndexList list = grid->selectionModel()->selectedIndexes();
    if(list.isEmpty())
        return;

    qSort(list);

    htmlData.append(lit("<html>\n"));
    htmlData.append(lit("<body>\n"));
    htmlData.append(lit("<table>\n"));

    htmlData.append(lit("<tr>"));
    for(int i = 0; i < list.size() && list[i].row() == list[0].row(); ++i)
    {
        if (i > 0)
            textData.append(textDelimiter);
        QString header = model->headerData(list[i].column(), Qt::Horizontal).toString();
        htmlData.append(lit("<th>"));
        htmlData.append(header);
        htmlData.append(lit("</th>"));
        textData.append(header);
    }
    htmlData.append(lit("</tr>"));

    int prevRow = -1;
    for(int i = 0; i < list.size(); ++i)
    {
        if(list[i].row() != prevRow) {
            prevRow = list[i].row();
            textData.append(lit('\n'));
            if (i > 0)
                htmlData.append(lit("</tr>"));
            htmlData.append(lit("<tr>"));
        }
        else {
            textData.append(textDelimiter);
        }

        htmlData.append(lit("<td>"));
        htmlData.append(model->data(list[i], Qn::DisplayHtmlRole).toString());
        htmlData.append(lit("</td>"));

        textData.append(model->data(list[i]).toString());
    }
    htmlData.append(lit("</tr>\n"));
    htmlData.append(lit("</table>\n"));
    htmlData.append(lit("</body>\n"));
    htmlData.append(lit("</html>\n"));
    textData.append(lit('\n'));
}
