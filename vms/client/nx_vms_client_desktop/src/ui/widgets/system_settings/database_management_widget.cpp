// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "database_management_widget.h"
#include "ui_database_management_widget.h"

#include <QtGui/QDesktopServices>

#include <api/server_rest_connection.h>
#include <common/common_module.h>
#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/settings/local_settings.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/system_logon/logic/fresh_session_token_helper.h>
#include <nx_ec/abstract_ec_connection.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/dialogs/common/file_messages.h>
#include <ui/dialogs/common/session_aware_dialog.h>
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
    ui->progressBar->setTextVisible(false);
    ui->openFolderButton->setIcon(qnSkin->icon("text_buttons/newfolder.svg"));
    ui->openFolderButton->setFlat(true);

    setHelpTopic(this, HelpTopic::Id::SystemSettings_Server_Backup);

    connect(ui->backupButton, &QPushButton::clicked, this, &QnDatabaseManagementWidget::backupDb);
    connect(ui->restoreButton, &QPushButton::clicked, this, &QnDatabaseManagementWidget::restoreDb);

    updateVisible();
}

QnDatabaseManagementWidget::~QnDatabaseManagementWidget()
{
}

void QnDatabaseManagementWidget::backupDb()
{
    QScopedPointer<QnCustomFileDialog> fileDialog(new QnCustomFileDialog(
        this,
        tr("Save Database Backup..."),
        appContext()->localSettings()->lastDatabaseBackupDir(),
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
    appContext()->localSettings()->lastDatabaseBackupDir = fileInfo.absolutePath();

    if (fileInfo.exists() && !fileInfo.isWritable())
    {
        QnFileMessages::overwriteFailed(this, fileName);
        return;
    }

    resetStyle(ui->labelMessage);
    ui->labelMessage->setText(tr("Database backup is being downloaded from the server. Please wait."));

    auto dumpDatabaseHandler = nx::utils::guarded(this,
        [this, fileName](
            bool success,
            rest::Handle,
            rest::ErrorOrData<QByteArray> errorOrData)
        {
            success = false;
            if (auto data = std::get_if<QByteArray>(&errorOrData))
            {
                QFile file(fileName);
                success = file.open(QIODevice::WriteOnly) && file.write(*data) == data->size();
            }

            if (success)
            {
                ui->labelMessage->setText(tr("Database backed up to file"));
            }
            else
            {
                if (auto error = std::get_if<nx::network::rest::Result>(&errorOrData))
                {
                    NX_ERROR(
                        this, "Failed to dump Server database: %1", QJson::serialized(*error));
                }
                setWarningStyle(ui->labelMessage);
                ui->labelMessage->setText(tr("Failed to back up database"));
            }
            m_state = State::backupFinished;
            updateVisible(success);
        });
    m_state = State::backupStarted;
    updateVisible();

    auto sessionTokenHelper = FreshSessionTokenHelper::makeHelper(
        this,
        tr("Save Database Backup"),
        tr("Enter your account password to create backup"),
        tr("Create"),
        FreshSessionTokenHelper::ActionType::backup);

    auto handle = connection->dumpDatabase(
        sessionTokenHelper, std::move(dumpDatabaseHandler), thread());

    connect(
        ui->cancelCreateBackupButton,
        &QPushButton::clicked,
        this,
        [handle, connection]() { connection->cancelRequest(handle); });

    connect(
        ui->openFolderButton,
        &QPushButton::clicked,
        this,
        [path = fileInfo.absolutePath()]
        {
            if (!NX_ASSERT(!path.isEmpty()))
                return;

            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        });
}

void QnDatabaseManagementWidget::restoreDb()
{
    QString fileName = QFileDialog::getOpenFileName(
        this,
        tr("Open Database Backup..."),
        appContext()->localSettings()->lastDatabaseBackupDir(),
        kDbFilesFilter,
        nullptr,
        QnCustomFileDialog::fileDialogOptions());
    if (fileName.isEmpty())
        return;

    // Check if we were disconnected (server shut down) while the dialog was open.
    auto connection = connectedServerApi();
    if (!connection)
        return;

    appContext()->localSettings()->lastDatabaseBackupDir = QFileInfo(fileName).absolutePath();

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

    auto data = file.readAll();
    file.close();

    resetStyle(ui->labelMessage);
    ui->labelMessage->setText(tr("Database backup is being uploaded to the server. Please wait."));

    auto restoreDatabaseHandler = nx::utils::guarded(this,
        [this, fileName](bool, rest::Handle, rest::ErrorOrEmpty reply)
        {
            m_state = State::restoreFinished;
            updateVisible();
            if (std::holds_alternative<rest::Empty>(reply))
            {
                ui->labelMessage->setText(tr(
                    "Database successfully restored. Server application will restart shortly."));
                return;
            }

            NX_ERROR(this,
                "Failed to restore Server database from file '%1'. %2",
                fileName,
                std::get<nx::network::rest::Result>(reply).errorString);
            setWarningStyle(ui->labelMessage);
            ui->labelMessage->setText(tr("Failed to restore database"));
        });

    m_state = State::restoreStarted;
    updateVisible();

    auto sessionTokenHelper = FreshSessionTokenHelper::makeHelper(
        this,
        tr("Restore from Database Backup"),
        tr("Enter your account password to restore System from backup"),
        tr("Restore"),
        FreshSessionTokenHelper::ActionType::restore);

    auto handle = connection->restoreDatabase(
        sessionTokenHelper, data, std::move(restoreDatabaseHandler), thread());
    connect(
        ui->cancelRestoreBackupButton,
        &QPushButton::clicked,
        this,
        [handle, connection]() { connection->cancelRequest(handle); });
}

void QnDatabaseManagementWidget::updateVisible(bool operationSuccess)
{
    switch (m_state)
    {
        case State::empty:
        {
            ui->frameProgress->hide();
            ui->cancelCreateBackupButton->hide();
            ui->cancelRestoreBackupButton->hide();
            ui->openFolderButton->hide();
            break;
        }
        case State::backupStarted:
        {
            ui->progressBar->setRange(0,0);
            ui->cancelRestoreBackupButton->hide();
            ui->cancelCreateBackupButton->show();
            ui->openFolderButton->hide();
            ui->frameMain->setDisabled(true);
            ui->frameProgress->show();
            break;
        }
        case State::backupFinished:
        {
            ui->progressBar->setRange(0, 100);
            ui->progressBar->setValue(100);
            ui->frameMain->setDisabled(false);
            ui->cancelCreateBackupButton->hide();
            if (operationSuccess)
                ui->openFolderButton->show();

            break;
        }
        case State::restoreStarted:
        {
            ui->progressBar->setRange(0, 0);
            ui->cancelCreateBackupButton->hide();
            ui->cancelRestoreBackupButton->show();
            ui->openFolderButton->hide();
            ui->frameMain->setDisabled(true);
            ui->frameProgress->show();
            break;
        }
        case State::restoreFinished:
        {
            ui->progressBar->setRange(0, 100);
            ui->progressBar->setValue(100);
            ui->frameMain->setDisabled(false);
            ui->openFolderButton->hide();
            ui->cancelRestoreBackupButton->hide();
            break;
        }

        default:
            break;
    }
}

void QnDatabaseManagementWidget::setReadOnlyInternal(bool readOnly)
{
    ui->restoreButton->setEnabled(!readOnly);
}

void QnDatabaseManagementWidget::hideEvent(QHideEvent* event)
{
    base_type::hideEvent(event);
    if (m_state == State::backupFinished || m_state == State::restoreFinished)
    {
        m_state = State::empty;
        updateVisible();
    }
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
