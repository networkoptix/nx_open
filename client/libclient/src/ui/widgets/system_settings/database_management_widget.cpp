#include "database_management_widget.h"
#include "ui_database_management_widget.h"

#include <QtCore/QFileInfo>

#include <nx/utils/log/log.h>

#include "client/client_settings.h"
#include "api/app_server_connection.h"

#include <ui/actions/actions.h>
#include <ui/actions/action_manager.h>
#include <ui/actions/action_parameters.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/dialogs/common/progress_dialog.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/file_dialog.h>
#include <ui/workbench/workbench_context.h>

#include <ui/dialogs/common/session_aware_dialog.h>

namespace {
const QLatin1String dbExtension(".db");
}

QnDatabaseManagementWidget::QnDatabaseManagementWidget(QWidget *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::DatabaseManagementWidget())
{
    ui->setupUi(this);
    ui->labelWidget->setText(tr("You can create a backup for system configurations that can be restored in case of failure."));

    setHelpTopic(this, Qn::SystemSettings_Server_Backup_Help);

    connect(ui->backupButton, &QPushButton::clicked, this, &QnDatabaseManagementWidget::backupDb);
    connect(ui->restoreButton, &QPushButton::clicked, this, &QnDatabaseManagementWidget::restoreDb);
}

QnDatabaseManagementWidget::~QnDatabaseManagementWidget()
{
    return;
}

void QnDatabaseManagementWidget::backupDb()
{
    // TODO: #dklychkov file name filter string duplicates the value of dbExtension variable
    QScopedPointer<QnCustomFileDialog> fileDialog(new QnCustomFileDialog(
        this,
        tr("Save Database Backup..."),
        qnSettings->lastDatabaseBackupDir(),
        tr("Database Backup Files (*.db)")));
    fileDialog->setAcceptMode(QFileDialog::AcceptSave);
    if (!fileDialog->exec())
        return;

    QString fileName = fileDialog->selectedFile();
    if (fileName.isEmpty())
        return;

    /* Check if we were disconnected (server shut down) while the dialog was open. */
    if (!context()->user())
        return;

    qnSettings->setLastDatabaseBackupDir(QFileInfo(fileName).absolutePath());
    if (!fileName.endsWith(dbExtension))
        fileName += dbExtension;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly))
    {
        QnMessageBox::critical(
            this,
            tr("Error"),
            tr("Could not open file '%1' for writing.").arg(fileName));
        return;
    }

    //TODO: #GDM QnSessionAwareDialog, QScopedPointer vs QObject-parent problem
    QScopedPointer<QnProgressDialog> dialog(new QnProgressDialog());
    dialog->setMinimum(0);
    dialog->setMaximum(0);
    dialog->setWindowTitle(tr("Downloading Database Backup"));
    dialog->setLabelText(tr("Database backup is being downloaded from the server. Please wait."));
    dialog->setModal(true);

    ec2::ErrorCode errorCode;
    QByteArray databaseData;
    auto dumpDatabaseHandler =
        [&dialog, &errorCode, &databaseData]
        (int /*reqID*/, ec2::ErrorCode _errorCode, const ec2::ApiDatabaseDumpData& dbData)
        {
            errorCode = _errorCode;
            databaseData = dbData.data;
            dialog->reset();
        };
    QnAppServerConnectionFactory::getConnection2()->dumpDatabaseAsync(dialog.data(), dumpDatabaseHandler);
    dialog->exec();
    if (dialog->wasCanceled())
        return;    //TODO: #ak is running request finish OK?

    if (errorCode != ec2::ErrorCode::ok)
    {
        NX_LOG(lit("Failed to dump Server database: %1").arg(ec2::toString(errorCode)), cl_logERROR);
        QnMessageBox::information(
            this,
            tr("Information"),
            tr("Failed to dump server database to %1.").arg(fileName));
        return;
    }

    file.write(databaseData);
    file.close();

    QnMessageBox::information(
        this,
        tr("Information"),
        tr("Database was successfully backed up into file '%1'.").arg(fileName));
}

void QnDatabaseManagementWidget::restoreDb()
{
    QString fileName = QnFileDialog::getOpenFileName(
        this,
        tr("Open Database Backup..."),
        qnSettings->lastDatabaseBackupDir(),
        tr("Database Backup Files (*.db)"),
        NULL,
        QnCustomFileDialog::fileDialogOptions());
    if (fileName.isEmpty())
        return;

    /* Check if we were disconnected (server shut down) while the dialog was open. */
    if (!context()->user())
        return;

    qnSettings->setLastDatabaseBackupDir(QFileInfo(fileName).absolutePath());

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
    {
        QnMessageBox::critical(
            this,
            tr("Error"),
            tr("Could not open file '%1' for reading.").arg(fileName));
        return;
    }

    if (QnMessageBox::warning(this, tr("Warning"), tr("Are you sure you would like to restore the database? All existing data will be lost."),
                             QDialogButtonBox::Ok, QDialogButtonBox::Cancel) == QDialogButtonBox::Cancel) {
        file.close();
        return;
    }

    ec2::ApiDatabaseDumpData data;
    data.data = file.readAll();
    file.close();

    //TODO: #GDM QnSessionAwareDialog, QScopedPointer vs QObject-parent problem
    QScopedPointer<QnProgressDialog> dialog(new QnProgressDialog);
    dialog->setMinimum(0);
    dialog->setMaximum(0);
    dialog->setWindowTitle(tr("Restoring Database Backup"));
    dialog->setLabelText(tr("Database backup is being uploaded to the server. Please wait."));
    dialog->setModal(true);

    ec2::ErrorCode errorCode;
    auto restoreDatabaseHandler =
        [&dialog, &errorCode](int /*reqID*/, ec2::ErrorCode _errorCode)
        {
            errorCode = _errorCode;
            dialog->reset();
        };
    ec2::AbstractECConnectionPtr conn = QnAppServerConnectionFactory::getConnection2();
    if (!conn)
    {
        QnMessageBox::information(this,
            tr("Information"),
            tr("You need to connect to a server prior to backup start."));
        return;
    }

    conn->restoreDatabaseAsync(data, dialog.data(), restoreDatabaseHandler);
    dialog->exec();
    if (dialog->wasCanceled())
        return; // TODO: #Elric make non-cancelable.   TODO: #ak is running request finish OK?

    if (errorCode == ec2::ErrorCode::ok)
    {
        QnMessageBox::information(this,
            tr("Information"),
            tr("Database was successfully restored from file '%1'. Server will be restarted.")
                .arg(fileName));
        menu()->trigger(QnActions::ReconnectAction);
    }
    else
    {
        NX_LOG(lit("Failed to restore Server database from file '%1'. %2")
            .arg(fileName)
            .arg(ec2::toString(errorCode)),
            cl_logERROR);
        QnMessageBox::critical(
            this,
            tr("Error"),
            tr("An error has occurred while restoring the database from file '%1'.")
                .arg(fileName));
    }
}

void QnDatabaseManagementWidget::setReadOnlyInternal(bool readOnly)
{
    ui->restoreButton->setEnabled(!readOnly);
}

void QnDatabaseManagementWidget::applyChanges()
{
    /* Widget is read-only */
}

void QnDatabaseManagementWidget::loadDataToUi()
{
    /* Widget is read-only */
}

bool QnDatabaseManagementWidget::hasChanges() const
{
    return false;
}
