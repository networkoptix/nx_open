#include "database_management_widget.h"
#include "ui_database_management_widget.h"

#include <ui/dialogs/progress_dialog.h>

#include "utils/settings.h"
#include "api/app_server_connection.h"

QnDatabaseManagementWidget::QnDatabaseManagementWidget(QWidget *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    ui(new Ui::DatabaseManagementWidget())
{
    ui->setupUi(this);
    
    connect(ui->backupButton, SIGNAL(clicked()), this, SLOT(at_backupButton_clicked()));
    connect(ui->restoreButton, SIGNAL(clicked()), this, SLOT(at_restoreButton_clicked()));
}

QnDatabaseManagementWidget::~QnDatabaseManagementWidget() {
    return;
}

void QnDatabaseManagementWidget::at_backupButton_clicked() {
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save Database Backup..."), qnSettings->lastDatabaseBackupDir(), tr("Database Backup Files (*.bin)"), &fileName);
    if(fileName.isEmpty())
        return;

    QFile file(fileName);
    if(!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, tr("Error"), tr("Could not open file '%1' for writing."));
        return;
    }

    qnSettings->setLastDatabaseBackupDir(QFileInfo(fileName).absolutePath());
    
    QScopedPointer<QnProgressDialog> dialog(new QnProgressDialog);
    dialog->setMinimum(0);
    dialog->setMaximum(0);
    dialog->setWindowTitle(tr("Downloading Database Backup"));
    dialog->setLabelText(tr("Database backup is being downloaded from the server. Please wait."));
    dialog->setModal(true);

    QScopedPointer<QnDatabaseManagementWidgetReplyProcessor> processor(new QnDatabaseManagementWidgetReplyProcessor());
    connect(processor, SIGNAL(activated()), dialog, SLOT(close()));
    
    QnAppServerConnectionFactory::createConnection()->dumpDatabase(processor.data(), SLOT(activate(const QnHTTPRawResponse &, int)));
    dialog->exec();
    if(dialog->wasCanceled())
        return;

    file.write(processor->response.data);
    file.close();
}

void QnDatabaseManagementWidget::at_restoreButton_clicked() {


}

void QnDatabaseManagementWidget::at_replyReceived(const QnHTTPRawResponse &response, int handle) {

}
