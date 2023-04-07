// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "logs_management_widget.h"
#include "ui_logs_management_widget.h"

#include <QtCore/QStandardPaths>
#include <QtGui/QDesktopServices>
#include <QtGui/QPainter>
#include <QtWidgets/QFileDialog>

#include <nx/vms/client/desktop/common/widgets/checkable_header_view.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/style/skin.h>
#include <nx/vms/client/desktop/system_administration/delegates/logs_management_table_delegate.h>
#include <nx/vms/client/desktop/system_administration/models/logs_management_model.h>
#include <nx/vms/client/desktop/system_administration/widgets/log_settings_dialog.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/logic/fresh_session_token_helper.h>

namespace nx::vms::client::desktop {

LogsManagementWidget::LogsManagementWidget(
    SystemContext* context,
    QWidget* parent)
    :
    base_type(parent),
    SystemContextAware(context),
    ui(new Ui::LogsManagementWidget),
    m_watcher(context->logsManagementWatcher())
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

void LogsManagementWidget::showEvent(QShowEvent* event)
{
    base_type::showEvent(event);
    if (NX_ASSERT(m_watcher))
    {
        auto state = m_watcher->state();
        if(state == LogsManagementWatcher::State::finished
           || state == LogsManagementWatcher::State::hasErrors
           || state == LogsManagementWatcher::State::hasLocalErrors)
        {
            needUpdateBeforeClosing = true;
        }
        m_watcher->setUpdatesEnabled(true);
    }
}

void LogsManagementWidget::hideEvent(QHideEvent* event)
{
    base_type::hideEvent(event);
    if (NX_ASSERT(m_watcher))
    {
        if (needUpdateBeforeClosing)
            m_watcher->completeDownload();

        m_watcher->setUpdatesEnabled(false);
    }
}

void LogsManagementWidget::setupUi()
{
    ui->setupUi(this);

    setWarningStyle(ui->errorLabel);

    connect(
        m_watcher, &LogsManagementWatcher::stateChanged,
        this, &LogsManagementWidget::updateWidgets);

    connect(
        m_watcher, &LogsManagementWatcher::progressChanged, this,
        [this](double progress)
        {
            ui->progressBar->setValue(100 * progress); //< The range is [0..100].
        });

    ui->unitsTable->setModel(new LogsManagementModel(this, m_watcher));
    ui->unitsTable->setItemDelegate(new LogsManagementTableDelegate(this));

    auto header = new CheckableHeaderView(LogsManagementModel::Columns::CheckBoxColumn, this);
    ui->unitsTable->setHorizontalHeader(header);
    header->setSectionResizeMode(
        LogsManagementModel::Columns::NameColumn, QHeaderView::Stretch);
    header->setSectionResizeMode(
        LogsManagementModel::Columns::LogLevelColumn, QHeaderView::Stretch);
    header->setSectionResizeMode(
        LogsManagementModel::Columns::CheckBoxColumn, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(
        LogsManagementModel::Columns::StatusColumn, QHeaderView::ResizeToContents);

    ui->downloadButton->setIcon(qnSkin->icon("text_buttons/download.png"));
    ui->settingsButton->setIcon(qnSkin->icon("text_buttons/settings.png"));
    ui->resetButton->setIcon(qnSkin->icon("text_buttons/refresh.png"));
    ui->openFolderButton->setIcon(qnSkin->icon("text_buttons/newfolder.svg"));

    ui->resetButton->setFlat(true);
    ui->openFolderButton->setFlat(true);

    connect(ui->unitsTable->horizontalHeader(), &QHeaderView::sectionClicked, this,
        [this](int logicalIndex)
        {
            if (!NX_ASSERT(m_watcher))
                return;

            if (logicalIndex != LogsManagementModel::Columns::CheckBoxColumn)
                return;

            m_watcher->setAllItemsAreChecked(m_watcher->itemsCheckState() == Qt::Unchecked);
        });

    connect(ui->unitsTable, &QTableView::clicked, this,
        [this](const QModelIndex& index)
        {
            if (ui->unitsTable->isColumnHidden(LogsManagementModel::Columns::CheckBoxColumn))
                return;

            auto model = ui->unitsTable->model();
            const auto checkBoxIndex =
                index.siblingAtColumn(LogsManagementModel::Columns::CheckBoxColumn);

            auto state = model->data(checkBoxIndex, Qt::CheckStateRole).value<Qt::CheckState>();
            state = (state == Qt::CheckState::Checked)
                ? Qt::CheckState::Unchecked
                : Qt::CheckState::Checked;
            model->setData(checkBoxIndex, state, Qt::CheckStateRole);

            ui->unitsTable->update(checkBoxIndex);
        });

    connect(m_watcher, &LogsManagementWatcher::selectionChanged, this,
        [this, header](Qt::CheckState state)
        {
            header->setCheckState(state);
        });

    connect(ui->settingsButton, &QPushButton::clicked, this,
        [this]
        {
            if (!NX_ASSERT(m_watcher))
                return;

            auto dialog = QScopedPointer(new LogSettingsDialog());
            dialog->init(m_watcher->checkedItems());

            if (dialog->exec() == QDialog::Rejected)
                return;

            if (!dialog->hasChanges())
                return;

            m_watcher->applySettings(dialog->changes(), this);
        });

    connect(ui->resetButton, &QPushButton::clicked, this,
        [this]
        {
            if (!NX_ASSERT(m_watcher))
                return;

            m_watcher->applySettings(ConfigurableLogSettings::defaults(), this);
        });

    connect(ui->downloadButton, &QPushButton::clicked, this,
        [this]
        {
            if (!NX_ASSERT(m_watcher))
                return;

            QString dir = QFileDialog::getExistingDirectory(
                this,
                tr("Select Folder..."),
                QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
                QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

            if (!dir.isEmpty())
                m_watcher->startDownload(dir);
        });

    connect(ui->openFolderButton, &QPushButton::clicked, this,
        [this]
        {
            if (!NX_ASSERT(m_watcher))
                return;

            const auto path = m_watcher->path();
            if (!NX_ASSERT(!path.isEmpty()))
                return;

            QDesktopServices::openUrl(path);
        });

    connect(
        ui->cancelButton, &QPushButton::clicked,
        m_watcher, &LogsManagementWatcher::cancelDownload);

    connect(
        ui->retryButton, &QPushButton::clicked,
        m_watcher, &LogsManagementWatcher::restartFailed);

    connect(
        ui->doneButton, &QPushButton::clicked,
        m_watcher, &LogsManagementWatcher::completeDownload);

    updateWidgets(LogsManagementWatcher::State::empty); // TODO: get actual state.
}

void LogsManagementWidget::updateWidgets(LogsManagementWatcher::State state)
{
    bool hasSelection = false;
    bool downloadStarted = false;
    bool downloadFinished = false;
    bool hasErrors = false;
    bool hasLocalErrors = false;

    switch (state)
    {
        case LogsManagementWatcher::State::hasLocalErrors:
            hasLocalErrors = true;
            [[fallthrough]];

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

    needUpdateBeforeClosing = false;
    if(downloadFinished && isVisible())
        needUpdateBeforeClosing = true;

    ui->stackedWidget->setCurrentWidget(downloadStarted ? ui->activePage : ui->defaultPage);

    ui->unitsTable->setColumnHidden(LogsManagementModel::Columns::CheckBoxColumn, downloadStarted);
    ui->unitsTable->setColumnHidden(LogsManagementModel::Columns::StatusColumn, !downloadStarted);

    ui->downloadButton->setEnabled(hasSelection);
    ui->settingsButton->setEnabled(hasSelection);
    ui->resetButton->setEnabled(hasSelection);

    ui->cancelButton->setVisible(!downloadFinished);
    ui->errorLabel->setVisible(downloadFinished && hasLocalErrors);
    ui->finishedLabel->setVisible(downloadFinished && !hasLocalErrors);
    ui->doneButton->setVisible(downloadFinished);
    ui->retryButton->setVisible(hasErrors);
}

} // namespace nx::vms::client::desktop
