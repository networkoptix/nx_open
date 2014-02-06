#include "database_management_widget.h"
#include "ui_database_management_widget.h"

#include <QtWidgets/QMessageBox>
#include <QtWidgets/QFileDialog>
#include <QtCore/QFileInfo>

#include "client/client_settings.h"
#include "api/app_server_connection.h"

#include <ui/actions/actions.h>
#include <ui/actions/action_manager.h>
#include <ui/actions/action_parameters.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/dialogs/progress_dialog.h>
#include <ui/dialogs/custom_file_dialog.h>
#include <ui/workbench/workbench_context.h>

QnDatabaseManagementWidget::QnDatabaseManagementWidget(QWidget *parent, Qt::WindowFlags windowFlags):
    base_type(parent, windowFlags),
    QnWorkbenchContextAware(parent),
    ui(new Ui::DatabaseManagementWidget())
{
    ui->setupUi(this);
    
    setHelpTopic(this, Qn::SystemSettings_Server_Backup_Help);

    connect(ui->backupButton, SIGNAL(clicked()), this, SLOT(at_backupButton_clicked()));
    connect(ui->restoreButton, SIGNAL(clicked()), this, SLOT(at_restoreButton_clicked()));
}

QnDatabaseManagementWidget::~QnDatabaseManagementWidget() {
    return;
}

void QnDatabaseManagementWidget::at_backupButton_clicked() {
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save Database Backup..."), qnSettings->lastDatabaseBackupDir(), tr("Database Backup Files (*.db)"), NULL, QnCustomFileDialog::fileDialogOptions());
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

    ec2::ErrorCode errorCode;
    QByteArray databaseData;
    auto dumpDatabaseHandler = 
        [&dialog, &errorCode, &databaseData]( int /*reqID*/, ec2::ErrorCode _errorCode, const QByteArray& dbData ) {
            errorCode = _errorCode;
            databaseData = dbData;
            dialog->reset(); 
    };
    QnAppServerConnectionFactory::createConnection2Sync()->dumpDatabaseAsync( dialog.data(), dumpDatabaseHandler );
    dialog->exec();
    if(dialog->wasCanceled())
        return;    //TODO: #ak is running request finish OK?

    if( errorCode != ec2::ErrorCode::ok )
    {
        NX_LOG( lit("Failed to dump EC database: %1").arg(ec2::toString(errorCode)), cl_logERROR );
        QMessageBox::information(this, tr("Information"), tr("Failed to dump EC database to '%1'").arg(fileName));
        return;
    }

    file.write( databaseData );
    file.close();

    QMessageBox::information(this, tr("Information"), tr("Database was successfully backed up into file '%1'.").arg(fileName));
}

void QnDatabaseManagementWidget::at_restoreButton_clicked() {
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open Database Backup..."), qnSettings->lastDatabaseBackupDir(), tr("Database Backup Files (*.db)"), NULL, QnCustomFileDialog::fileDialogOptions());
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

    ec2::ErrorCode errorCode;
    auto restoreDatabaseHandler = 
        [&dialog, &errorCode]( int /*reqID*/, ec2::ErrorCode _errorCode ) {
            errorCode = _errorCode;
            dialog->reset(); 
    };
    QnAppServerConnectionFactory::createConnection2Sync()->restoreDatabaseAsync( data, dialog.data(), restoreDatabaseHandler );
    dialog->exec();
    if(dialog->wasCanceled())
        return; // TODO: #Elric make non-cancellable.   TODO: #ak is running request finish OK?

    if( errorCode == ec2::ErrorCode::ok ) {
        QMessageBox::information(this,
                                 tr("Information"),
                                 tr("Database was successfully restored from file '%1'.").arg(fileName));
        menu()->trigger(Qn::ReconnectAction);
    } else {
        NX_LOG( lit("Failed to restore EC database from file '$1'. $2").arg(fileName).arg(ec2::toString(errorCode)), cl_logERROR );
        QMessageBox::critical(this, tr("Error"), tr("An error has occured while restoring the database from file '%1'.")
                              .arg(fileName));
    }
}
