// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "database_management_widget.h"
#include "ui_database_management_widget.h"

#include <api/server_rest_connection.h>
#include <client/client_settings.h>
#include <common/common_module.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/desktop/common/dialogs/progress_dialog.h>
#include <nx/vms/client/desktop/system_logon/logic/fresh_session_token_helper.h>
#include <nx_ec/abstract_ec_connection.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/file_messages.h>
#include <ui/dialogs/common/session_aware_dialog.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/workbench/workbench_context.h>

using namespace nx::vms::client::desktop;

namespace {

static const QString kDbExtension = ".db";

static const QString kDbFilesFilter = QnCustomFileDialog::createFilter(
    QnDatabaseManagementWidget::tr("Database Backup Files"), kDbExtension.mid(1));

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

    // Check if we were disconnected (server shut down) while the dialog was open.
    auto connection = connectedServerApi();
    if (!connection)
        return;

    if (!fileName.endsWith(kDbExtension))
        fileName += kDbExtension;

    const QFileInfo fileInfo(fileName);
    qnSettings->setLastDatabaseBackupDir(fileInfo.absolutePath());

    if (fileInfo.exists() && !fileInfo.isWritable())
    {
        QnFileMessages::overwriteFailed(this, fileName);
        return;
    }

    auto ownerSessionToken = FreshSessionTokenHelper(this).getToken(
        tr("Save Database Backup"),
        tr("Enter your account password to create backup"),
        tr("Create"),
        FreshSessionTokenHelper::ActionType::backup);
    if (ownerSessionToken.empty())
        return;

    QPointer<ProgressDialog> dialog(new ProgressDialog(this));
    dialog->setInfiniteMode();
    dialog->setWindowTitle(tr("Downloading Database Backup"));
    dialog->setText(tr("Database backup is being downloaded from the server. Please wait."));
    dialog->setModal(true);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    auto dumpDatabaseHandler = nx::utils::guarded(
        dialog,
        [this, dialog, fileName](
            bool success,
            rest::Handle,
            rest::ErrorOrData<nx::vms::api::DatabaseDumpData> errorOrData)
        {
            success = false;
            if (auto data = std::get_if<nx::vms::api::DatabaseDumpData>(&errorOrData))
            {
                QFile file(fileName);
                success = file.open(QIODevice::WriteOnly)
                    && file.write(data->data) == data->data.size();
            }

            dialog->accept();

            if (success)
            {
                QnSessionAwareMessageBox::success(
                    this, tr("Database backed up to file"), fileName);
            }
            else
            {
                if (auto error = std::get_if<nx::network::rest::Result>(&errorOrData))
                {
                    NX_ERROR(
                        this, "Failed to dump Server database: %1", QJson::serialized(*error));
                }
                QnSessionAwareMessageBox::critical(this, tr("Failed to back up database"));
            }
        });

    dialog->open();
    auto handle = connection->dumpDatabase(
        ownerSessionToken.value, std::move(dumpDatabaseHandler), thread());
    connect(
        dialog,
        &ProgressDialog::canceled,
        this,
        [handle, connection]() { connection->cancelRequest(handle); });
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

    // Check if we were disconnected (server shut down) while the dialog was open.
    auto connection = connectedServerApi();
    if (!connection)
        return;

    qnSettings->setLastDatabaseBackupDir(QFileInfo(fileName).absolutePath());

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
    {
        QnSessionAwareMessageBox::critical(this, tr("Failed to open file"), fileName);
        return;
    }

    const auto button = QnSessionAwareMessageBox::question(
        this,
        tr("Restore database?"),
        tr("System configuration will be restored from backup,"
            " Server application will be restarted."),
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel,
        QDialogButtonBox::Ok);

    if (button != QDialogButtonBox::Ok)
        return;

    auto ownerSessionToken = FreshSessionTokenHelper(this).getToken(
        tr("Restore from Database Backup"),
        tr("Enter your account password to restore System from backup"),
        tr("Restore"),
        FreshSessionTokenHelper::ActionType::restore);
    if (ownerSessionToken.empty())
        return;

    nx::vms::api::DatabaseDumpData data;
    data.data = file.readAll();
    file.close();

    QPointer<ProgressDialog> dialog(new ProgressDialog(this));
    dialog->setInfiniteMode();
    dialog->setWindowTitle(tr("Restoring Database Backup"));
    dialog->setText(tr("Database backup is being uploaded to the server. Please wait."));
    dialog->setModal(true);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    auto restoreDatabaseHandler = nx::utils::guarded(dialog,
        [this, dialog, fileName](bool, rest::Handle, rest::ErrorOrEmpty reply)
        {
            dialog->accept();
            if (std::holds_alternative<rest::Empty>(reply))
            {
                QnSessionAwareMessageBox::success(this,
                    tr("Database successfully restored"),
                    tr("Server application will restart shortly."));
                return;
            }

            NX_ERROR(this,
                "Failed to restore Server database from file '%1'. %2",
                fileName,
                std::get<nx::network::rest::Result>(reply).errorString);
            QnSessionAwareMessageBox::critical(this, tr("Failed to restore database"));
        });

    dialog->open();
    auto handle = connection->restoreDatabase(
        data, ownerSessionToken.value, std::move(restoreDatabaseHandler), thread());
    connect(
        dialog,
        &ProgressDialog::canceled,
        this,
        [handle, connection]() { connection->cancelRequest(handle); });
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
