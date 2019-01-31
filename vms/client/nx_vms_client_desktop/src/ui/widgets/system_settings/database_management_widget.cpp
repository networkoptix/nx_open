#include "database_management_widget.h"
#include "ui_database_management_widget.h"

#include <nx/utils/log/log.h>

#include <common/common_module.h>

#include "client/client_settings.h"

#include <nx_ec/ec_api.h>

#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/dialogs/common/progress_dialog.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/file_messages.h>
#include <ui/workbench/workbench_context.h>

#include <ui/dialogs/common/session_aware_dialog.h>

namespace {

const QLatin1String dbExtension(".db");

static const QString kDbFilesFilter = QnCustomFileDialog::createFilter(
    QnDatabaseManagementWidget::tr("Database Backup Files"), "db");

} // namespace

QnDatabaseManagementWidget::QnDatabaseManagementWidget(QWidget *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::DatabaseManagementWidget())
{
    ui->setupUi(this);

    setHelpTopic(this, Qn::SystemSettings_Server_Backup_Help);

    connect(ui->backupButton, &QPushButton::clicked, this, &QnDatabaseManagementWidget::backupDb);
    connect(ui->restoreButton, &QPushButton::clicked, this, &QnDatabaseManagementWidget::restoreDb);
}

QnDatabaseManagementWidget::~QnDatabaseManagementWidget()
{
}

void QnDatabaseManagementWidget::backupDb()
{
    // TODO: #dklychkov file name filter string duplicates the value of dbExtension variable
    QScopedPointer<QnCustomFileDialog> fileDialog(new QnCustomFileDialog(
        this,
        tr("Save Database Backup..."),
        qnSettings->lastDatabaseBackupDir(),
        kDbFilesFilter));
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
        QnFileMessages::overwriteFailed(this, fileName);
        return;
    }

    // TODO: #GDM QnSessionAware, QScopedPointer vs QObject-parent problem
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
        (int /*reqID*/, ec2::ErrorCode _errorCode, const nx::vms::api::DatabaseDumpData& dbData)
        {
            errorCode = _errorCode;
            databaseData = dbData.data;
            dialog->reset();
        };
    commonModule()->ec2Connection()->dumpDatabaseAsync(dialog.data(), dumpDatabaseHandler);
    dialog->exec();
    if (dialog->wasCanceled())
        return;    // TODO: #ak is running request finish OK?

    if (errorCode != ec2::ErrorCode::ok)
    {
        NX_ERROR(this, lit("Failed to dump Server database: %1").arg(ec2::toString(errorCode)));
        QnMessageBox::critical(this, tr("Failed to back up database"));
        return;
    }

    file.write(databaseData);
    file.close();

    QnMessageBox::success(this, tr("Database backed up to file"), fileName);
}

void QnDatabaseManagementWidget::restoreDb()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("Open Database Backup..."),
        qnSettings->lastDatabaseBackupDir(),
        kDbFilesFilter,
        nullptr,
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
        QnMessageBox::critical(this, tr("Failed to open file"), fileName);
        return;
    }

    const auto button = QnMessageBox::question(this,
        tr("Restore database?"),
        tr("System configuration will be restored from backup,"
           " Server application will be restarted."),
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, QDialogButtonBox::Ok);

    if (button != QDialogButtonBox::Ok)
    {
        file.close();
        return;
    }

    nx::vms::api::DatabaseDumpData data;
    data.data = file.readAll();
    file.close();

    // TODO: #GDM QnSessionAware, QScopedPointer vs QObject-parent problem
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
    ec2::AbstractECConnectionPtr conn = commonModule()->ec2Connection();
    if (!conn)
    {
        QnMessageBox::critical(this, tr("You need to connect to a server prior to backup start."));
        return;
    }

    conn->restoreDatabaseAsync(data, dialog.data(), restoreDatabaseHandler);
    dialog->exec();
    if (dialog->wasCanceled())
        return; // TODO: #Elric make non-cancelable.   TODO: #ak is running request finish OK?

    if (errorCode == ec2::ErrorCode::ok)
    {
        QnMessageBox::success(this,
            tr("Database successfully restored"),
            tr("Server application will restart shortly."));
    }
    else
    {
        NX_ERROR(this, lit("Failed to restore Server database from file '%1'. %2")
            .arg(fileName)
            .arg(ec2::toString(errorCode)));

        QnMessageBox::critical(this, tr("Failed to restore database"));
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
