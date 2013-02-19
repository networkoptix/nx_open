#include "database_management_widget.h"
#include "ui_database_management_widget.h"

#include <QMessageBox>

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
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save Database Backup..."), qnSettings->lastDatabaseBackupDir(), tr("Database Backup Files (*.db)"));
    if(fileName.isEmpty())
        return;
    qnSettings->setLastDatabaseBackupDir(QFileInfo(fileName).absolutePath());

    QFile file(fileName);
    if(!file.open(QIODevice::WriteOnly)) {
        QMessageBox::critical(this, tr("Error"), tr("Could not open file '%1' for writing.").arg(fileName));
        return;
    }
   
    QScopedPointer<QnProgressDialog> dialog(new QnProgressDialog);
    dialog->setMinimum(0);
    dialog->setMaximum(0);
    dialog->setWindowTitle(tr("Downloading Database Backup"));
    dialog->setLabelText(tr("Database backup is being downloaded from the server. Please wait."));
    dialog->setModal(true);

    QScopedPointer<QnDatabaseManagementWidgetReplyProcessor> processor(new QnDatabaseManagementWidgetReplyProcessor());
    connect(processor, SIGNAL(activated()), dialog, SLOT(reset()));
    
    QnAppServerConnectionFactory::createConnection()->dumpDatabase(processor.data(), SLOT(activate(const QnHTTPRawResponse &, int)));
    dialog->exec();
    if(dialog->wasCanceled())
        return;

    // TODO: #Elric check error code
    file.write(processor->response.data);
    file.close();

    QMessageBox::information(this, tr("Information"), tr("Database was successfully backed up into file '%1'.").arg(fileName));
}

void QnDatabaseManagementWidget::at_restoreButton_clicked() {
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open Database Backup..."), qnSettings->lastDatabaseBackupDir(), tr("Database Backup Files (*.db)"));
    if(fileName.isEmpty())
        return;
    qnSettings->setLastDatabaseBackupDir(QFileInfo(fileName).absolutePath());

    QFile file(fileName);
    if(!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, tr("Error"), tr("Could not open file '%1' for reading.").arg(fileName));
        return;
    }

    if (QMessageBox::warning(this, tr("Warning"), tr("Are you sure you want to start restoring database? All current data will be lost."),
                             QMessageBox::Ok, QMessageBox::Cancel) == QMessageBox::Cancel) {
        file.close();
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    QScopedPointer<QnProgressDialog> dialog(new QnProgressDialog);
    dialog->setMinimum(0);
    dialog->setMaximum(0);
    dialog->setWindowTitle(tr("Restoring Database Backup"));
    dialog->setLabelText(tr("Database backup is being uploaded to the server. Please wait."));
    dialog->setModal(true);

    QScopedPointer<QnDatabaseManagementWidgetReplyProcessor> processor(new QnDatabaseManagementWidgetReplyProcessor());
    connect(processor, SIGNAL(activated()), dialog, SLOT(reset()));

    QnAppServerConnectionFactory::createConnection()->restoreDatabase(data, processor.data(), SLOT(activate(const QnHTTPRawResponse &, int)));
    dialog->exec();
    if(dialog->wasCanceled())
        return; // TODO: #Elric make non-cancellable.

    if(processor->response.status == 0) {
        QMessageBox::information(this, tr("Information"), tr("Database was successfully restored from file '%1'.").arg(fileName));
    } else {
        QMessageBox::critical(this, tr("Error"), tr("An error has occured while restoring the database from file '%1'. Error description: '%2'.").arg(fileName).arg(QString::fromLatin1(processor->response.errorString)));
    }
}

