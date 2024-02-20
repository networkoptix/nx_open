// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "logs_management_widget.h"
#include "ui_logs_management_widget.h"

#include <algorithm>

#include <QtCore/QAbstractItemModel>
#include <QtCore/QStandardPaths>
#include <QtGui/QColor>
#include <QtGui/QDesktopServices>
#include <QtGui/QPainter>
#include <QtWidgets/QFileDialog>

#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/common/widgets/checkable_header_view.h>
#include <nx/vms/client/desktop/common/widgets/obtain_button.h>
#include <nx/vms/client/desktop/common/widgets/search_line_edit.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/system_administration/delegates/logs_management_table_delegate.h>
#include <nx/vms/client/desktop/system_administration/models/logs_management_model.h>
#include <nx/vms/client/desktop/system_administration/widgets/log_settings_dialog.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/client/desktop/system_logon/logic/fresh_session_token_helper.h>
#include <ui/common/palette.h>

namespace nx::vms::client::desktop {

namespace {

static const QColor kLight16Color = "#698796";
static const QColor kLight10Color = "#A5B7C0";
static const nx::vms::client::core::SvgIconColorer::IconSubstitutions kIconSubstitutions = {
    {QIcon::Normal, {{kLight16Color, "light16"}, {kLight10Color, "light4"}}},
    {QIcon::Active, {{kLight16Color, "light15"}, {kLight10Color, "light2"}}},
    {QIcon::Selected, {{kLight16Color, "light17"}, {kLight10Color, "light6"}}},
};

static const nx::vms::client::core::SvgIconColorer::IconSubstitutions kRetryIconSubstitutions = {
    {QIcon::Normal, {{kLight16Color, "light16"}}},
    {QIcon::Active, {{kLight16Color, "light17"}}},
};


constexpr auto kUpdateDelayMs = 1000;

} // namespace

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

    m_delayUpdateTimer.setSingleShot(true);
    m_delayUpdateTimer.setInterval(kUpdateDelayMs);
    connect(&m_delayUpdateTimer, &QTimer::timeout, this, &LogsManagementWidget::updateData);
}

LogsManagementWidget::~LogsManagementWidget()
{
    if (!NX_ASSERT(!isNetworkRequestRunning(), "Requests should already be completed."))
        discardChanges();
}

bool LogsManagementWidget::isNetworkRequestRunning() const
{
    return m_watcher->state() == LogsManagementWatcher::State::loading;
}

void LogsManagementWidget::discardChanges()
{
    if (isNetworkRequestRunning())
        m_watcher->cancelDownload();
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
            ui->downloadingPercentageLabel->setText(QString("%1%").arg((int)(100 * progress)));
        });

    ui->unitsTable->setModel(new LogsManagementModel(this, m_watcher));
    ui->unitsTable->setItemDelegate(new LogsManagementTableDelegate(this));

    auto header = new CheckableHeaderView(LogsManagementModel::Columns::CheckBoxColumn, this);
    ui->unitsTable->setHorizontalHeader(header);
    header->setSectionResizeMode(
        LogsManagementModel::Columns::NameColumn, QHeaderView::Stretch);
    header->setSectionResizeMode(
        LogsManagementModel::Columns::LogLevelColumn, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(
        LogsManagementModel::Columns::CheckBoxColumn, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(
        LogsManagementModel::Columns::StatusColumn, QHeaderView::ResizeToContents);
    connect(m_watcher, &LogsManagementWatcher::stateChanged,
        [header](LogsManagementWatcher::State state)
        {
            header->setEnabled(state == LogsManagementWatcher::State::empty
                || state == LogsManagementWatcher::State::hasSelection);
        });

    QFont font = ui->downloadButton->font();
    font.setBold(false);
    ui->downloadButton->setFont(font);
    ui->settingsButton->setFont(font);
    ui->resetButton->setFont(font);

    ui->downloadButton->setIcon(
        qnSkin->icon("text_buttons/download_20.svg", kIconSubstitutions));
    ui->downloadButton->setFlat(true);
    ui->settingsButton->setIcon(
        qnSkin->icon("text_buttons/settings_20.svg", kIconSubstitutions));
    ui->settingsButton->setFlat(true);
    ui->resetButton->setIcon(qnSkin->icon("text_buttons/reload_20.svg", kIconSubstitutions));
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

            if (model->data(checkBoxIndex, LogsManagementModel::EnabledRole).toBool())
            {
                auto state = model->data(checkBoxIndex, Qt::CheckStateRole).value<Qt::CheckState>();
                state = (state == Qt::CheckState::Checked)
                    ? Qt::CheckState::Unchecked
                    : Qt::CheckState::Checked;
                model->setData(checkBoxIndex, state, Qt::CheckStateRole);

                ui->unitsTable->update(checkBoxIndex);
            }
        });

    connect(ui->unitsTable->model(), &QAbstractTableModel::dataChanged,
        [this](const QModelIndex& topLeft, const QModelIndex& bottomRight,
            const QList<int>& )
        {
            addTableWidgets(topLeft, bottomRight);
            update();
        });

    connect(m_watcher, &LogsManagementWatcher::itemsChanged,
        [this]()
        {
            addTableWidgets({}, {});
            update();
        });

    connect(m_watcher, &LogsManagementWatcher::selectionChanged, this,
        [this, header](Qt::CheckState state)
        {
            header->setCheckState(state);
            updateWidgets(m_watcher->state());
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

            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
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

    connect(
        ui->selectedButton, &QPushButton::clicked,
        [this]
        {
            m_watcher->setAllItemsAreChecked(false);
        });

    static const QColor finishedColor = core::colorTheme()->color("brand_core");
    static const QColor errorColor = core::colorTheme()->color("attention.red");
    static const QString coloredTemplate = "<font color='%1'>%2</font>";

    ui->downloadingLabel->setText(coloredTemplate.arg(finishedColor.name(), tr("Downloading...")));
    ui->errorLabel->setText(coloredTemplate.arg(errorColor.name(), tr("Download completed with errors")));
    ui->finishedLabel->setText(coloredTemplate.arg(finishedColor.name(), tr("Download success")));
    ui->finishedIconButton->setText({});
    ui->finishedIconButton->setIconSize({16, 16});
    ui->finishedIconButton->setFixedSize({20, 20});
    ui->finishedIconButton->setFlat(true);
    ui->progressBar->setFixedHeight(4);

    setPaletteColor(ui->doneButton, QPalette::Button, finishedColor);
    setPaletteColor(ui->activePage, QPalette::Window, core::colorTheme()->color("dark6"));
    ui->activePage->setAutoFillBackground(true);

    ui->selectedButton->setFlat(true);
    ui->selectedButton->setIcon(qnSkin->icon("text_buttons/cross_close_20.svg", kIconSubstitutions));
    setPaletteColor(ui->selectedButton, QPalette::WindowText, core::colorTheme()->color("light4"));

    connect(ui->searchLineEdit, &SearchLineEdit::textChanged,
        this, &LogsManagementWidget::updateDataDelayed);

    updateWidgets(LogsManagementWatcher::State::empty); // TODO: get actual state.
}

void LogsManagementWidget::updateWidgets(LogsManagementWatcher::State state)
{
    int selectionCount = 0;
    bool downloadStarted = false;
    bool downloadFinished = false;
    int errorsCount = 0;
    bool hasLocalErrors = false;

    switch (state)
    {
        case LogsManagementWatcher::State::hasLocalErrors:
            hasLocalErrors = true;
            [[fallthrough]];

        case LogsManagementWatcher::State::hasErrors:
            errorsCount = m_watcher->itemsWithErrors().count();;
            [[fallthrough]];

        case LogsManagementWatcher::State::finished:
            downloadFinished = true;
            [[fallthrough]];

        case LogsManagementWatcher::State::loading:
            downloadStarted = true;
            [[fallthrough]];

        case LogsManagementWatcher::State::hasSelection:
            selectionCount = m_watcher->checkedItems().count();
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

    ui->unitsTable->setColumnHidden(LogsManagementModel::Columns::StatusColumn, !downloadStarted);

    ui->downloadButton->setVisible(selectionCount);
    ui->settingsButton->setVisible(selectionCount);
    ui->resetButton->setVisible(!selectionCount);
    ui->selectedButton->setVisible(selectionCount);
    ui->selectedButton->setText(tr("%n selected:", "number of selected rows", selectionCount));

    ui->cancelButton->setVisible(!downloadFinished);
    ui->downloadingLabel->setVisible(!downloadFinished);
    ui->downloadingPercentageLabel->setVisible(!downloadFinished);
    ui->errorLabel->setVisible(downloadFinished && errorsCount);
    ui->finishedIconButton->setVisible(downloadFinished);
    ui->finishedIconButton->setIcon(errorsCount != 0
        ? qnSkin->icon("text_buttons/error_16x16.svg")
        : qnSkin->icon("text_buttons/success_16x16.svg"));
    ui->finishedLabel->setVisible(downloadFinished && errorsCount == 0);
    ui->doneButton->setVisible(downloadFinished);
    ui->retryButton->setVisible(errorsCount);
    ui->retryButton->setText(tr("Retry (%1)").arg(errorsCount));
}

void LogsManagementWidget::addTableWidgets(const QModelIndex& topLeft, const QModelIndex& bottomRight)
{
    auto from = topLeft;
    auto to = bottomRight;
    if (!topLeft.isValid() || !bottomRight.isValid())
    {
        // Check entire table then
        from = ui->unitsTable->model()->index(0, 0);
        to = ui->unitsTable->model()->index(ui->unitsTable->model()->rowCount() - 1,
            ui->unitsTable->model()->columnCount() - 1);
    }

    for (int i = from.row(); i <= to.row(); ++i)
    {
        auto status = ui->unitsTable->model()->index(i, LogsManagementModel::StatusColumn);
        auto retry = ui->unitsTable->model()->index(i, LogsManagementModel::RetryColumn);

        if (status.data(LogsManagementModel::DownloadingRole).toBool())
        {
            if (ui->unitsTable->indexWidget(status) == nullptr)
            {
                auto busyButton = new ObtainButton("");
                busyButton->setFlat(true);
                busyButton->setFixedSize(20, 20);
                ui->unitsTable->setIndexWidget(status, std::move(busyButton));
            }
        }
        else
        {
            ui->unitsTable->setIndexWidget(status, nullptr);
        }

        if (retry.data(LogsManagementModel::RetryRole).toBool())
        {
            if (ui->unitsTable->indexWidget(retry) == nullptr)
            {
                auto* retryButton = new QPushButton(tr("Retry"));
                retryButton->setIcon(
                    qnSkin->icon("text_buttons/reload_20.svg", kRetryIconSubstitutions));
                retryButton->setFlat(true);
                auto id = nx::Uuid::fromString(retry.data(LogsManagementModel::QnUuidRole)
                    .value<QString>().toStdString());
                connect(retryButton,
                    &QPushButton::clicked,
                    [id, this]()
                    {
                        m_watcher->restartById(id);
                    });
                ui->unitsTable->setIndexWidget(retry, std::move(retryButton));
            }
        }
        else
        {
            ui->unitsTable->setIndexWidget(retry, nullptr);
        }
    }
}

void LogsManagementWidget::updateDataDelayed()
{
    m_delayUpdateTimer.start();
}

void LogsManagementWidget::updateData()
{
    if (auto model = dynamic_cast<LogsManagementModel*>(ui->unitsTable->model()))
        model->setFilterText(ui->searchLineEdit->text());
}

} // namespace nx::vms::client::desktop
