// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "logs_management_widget.h"
#include "ui_logs_management_widget.h"

#include <QtCore/QStandardPaths>
#include <QtGui/QDesktopServices>
#include <QtWidgets/QFileDialog>

#include <client_core/client_core_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <nx/vms/client/core/network/network_module.h>

#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/system_administration/delegates/logs_management_table_delegate.h>
#include <nx/vms/client/desktop/system_administration/models/logs_management_model.h>
#include <nx/vms/client/desktop/system_administration/watchers/logs_management_watcher.h>
#include <nx/vms/client/desktop/system_administration/widgets/log_settings_dialog.h>

namespace nx::vms::client::desktop {

LogsManagementWidget::LogsManagementWidget(QWidget* parent):
    base_type(parent),
    ui(new Ui::LogsManagementWidget)
{
    setupUi();
}

LogsManagementWidget::~LogsManagementWidget()
{
}

void LogsManagementWidget::loadDataToUi()
{
}

void LogsManagementWidget::applyChanges()
{
}

bool LogsManagementWidget::hasChanges() const
{
    return false;
}

void LogsManagementWidget::setReadOnlyInternal(bool readOnly)
{
}

void LogsManagementWidget::setupUi()
{
    ui->setupUi(this);

    auto m = new LogsManagementWatcher(this);

    connect(
        m, &LogsManagementWatcher::stateChanged,
        this, &LogsManagementWidget::updateWidgets);

    connect(
        m, &LogsManagementWatcher::progressChanged, this,
        [this](double progress)
        {
            ui->progressBar->setValue(100 * progress); //< The range is [0..100].
        });

    ui->unitsTable->setModel(new LogsManagementModel(this, m));
    ui->unitsTable->setItemDelegate(new LogsManagementTableDelegate(this));

    ui->unitsTable->horizontalHeader()->setSectionResizeMode(
        LogsManagementModel::Columns::NameColumn, QHeaderView::Stretch);
    ui->unitsTable->horizontalHeader()->setSectionResizeMode(
        LogsManagementModel::Columns::LogLevelColumn, QHeaderView::Stretch);
    ui->unitsTable->horizontalHeader()->setSectionResizeMode(
        LogsManagementModel::Columns::CheckBoxColumn, QHeaderView::ResizeToContents);
    ui->unitsTable->horizontalHeader()->setSectionResizeMode(
        LogsManagementModel::Columns::StatusColumn, QHeaderView::ResizeToContents);

    ui->downloadButton->setIcon(qnSkin->icon("text_buttons/download.png"));
    ui->settingsButton->setIcon(qnSkin->icon("text_buttons/settings.png"));
    ui->resetButton->setIcon(qnSkin->icon("text_buttons/refresh.png"));
    ui->openFolderButton->setIcon(qnSkin->icon("tree/local.png")); // FIXME

    ui->resetButton->setFlat(true);
    ui->openFolderButton->setFlat(true);

    connect(ui->settingsButton, &QPushButton::clicked, this,
        [this, m]
        {
            auto dialog = new LogSettingsDialog();
            dialog->init(m->checkedItems());

            if (dialog->exec() == QDialog::Rejected)
                return;

            if (!dialog->hasChanges())
                return;

            m->applySettings(dialog->changes());
        });

    connect(ui->resetButton, &QPushButton::clicked, this,
        [this, m]
        {
            m->applySettings(ConfigurableLogSettings::defaults());
        });

    connect(ui->downloadButton, &QPushButton::clicked, this,
        [this, m]
        {
            QString dir = QFileDialog::getExistingDirectory(
                this,
                tr("Select folder..."),
                QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
                QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

            if (!dir.isEmpty())
                m->startDownload(dir);
        });

    connect(ui->openFolderButton, &QPushButton::clicked, this,
        [this, m]
        {
            const auto path = m->path();
            if (!NX_ASSERT(!path.isEmpty()))
                return;

            QDesktopServices::openUrl(path);
        });

    connect(
        ui->cancelButton, &QPushButton::clicked,
        m, &LogsManagementWatcher::cancelDownload);

    connect(
        ui->retryButton, &QPushButton::clicked,
        m, &LogsManagementWatcher::restartFailed);

    connect(
        ui->doneButton, &QPushButton::clicked,
        m, &LogsManagementWatcher::completeDownload);

    updateWidgets(LogsManagementWatcher::State::empty); // TODO: get actual state.
}

void LogsManagementWidget::updateWidgets(LogsManagementWatcher::State state)
{
    bool hasSelection = false;
    bool downloadStarted = false;
    bool downloadFinished = false;
    bool hasErrors = false;

    switch (state)
    {
        case LogsManagementWatcher::State::hasErrors:
            hasErrors = true;
            [[fallthrough]];

        case LogsManagementWatcher::State::finished:
            downloadFinished = true;
            [[fallthrough]];

        case LogsManagementWatcher::State::loading:
            downloadStarted = true;
            [[fallthrough]];

        case LogsManagementWatcher::State::hasSelection:
            hasSelection = true;
            [[fallthrough]];

        case LogsManagementWatcher::State::empty:
            break;

        default:
            NX_ASSERT(false, "Unreachable");
    }

    ui->stackedWidget->setCurrentWidget(downloadStarted ? ui->activePage : ui->defaultPage);

    ui->unitsTable->setColumnHidden(LogsManagementModel::Columns::CheckBoxColumn, downloadStarted);
    ui->unitsTable->setColumnHidden(LogsManagementModel::Columns::StatusColumn, !downloadStarted);

    ui->downloadButton->setEnabled(hasSelection);
    ui->settingsButton->setEnabled(hasSelection);
    ui->resetButton->setEnabled(hasSelection);

    ui->cancelButton->setVisible(!downloadFinished);
    ui->finishedLabel->setVisible(downloadFinished);
    ui->doneButton->setVisible(downloadFinished);
    ui->retryButton->setVisible(hasErrors);
}

} // namespace nx::vms::client::desktop
