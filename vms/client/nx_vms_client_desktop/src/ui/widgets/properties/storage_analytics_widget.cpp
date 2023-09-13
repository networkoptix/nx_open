// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "storage_analytics_widget.h"
#include "ui_storage_analytics_widget.h"

#include <chrono>

#include <QtCore/QScopedValueRollback>
#include <QtGui/QClipboard>
#include <QtGui/QShowEvent>
#include <QtWidgets/QMenu>

#include <client/client_globals.h>
#include <common/common_module.h>
#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <nx/vms/client/core/skin/color_theme.h>
#include <nx/vms/client/core/skin/skin.h>
#include <nx/vms/client/desktop/common/utils/widget_anchor.h>
#include <nx/vms/client/desktop/common/widgets/dropdown_button.h>
#include <nx/vms/client/desktop/common/widgets/hint_button.h>
#include <nx/vms/client/desktop/common/widgets/snapped_scroll_bar.h>
#include <nx/vms/client/desktop/help/help_topic.h>
#include <nx/vms/client/desktop/help/help_topic_accessor.h>
#include <nx/vms/client/desktop/resource_views/data/resource_tree_globals.h>
#include <nx/vms/client/desktop/style/custom_style.h>
#include <nx/vms/client/desktop/ui/actions/action.h>
#include <nx/vms/client/desktop/ui/actions/action_manager.h>
#include <nx/vms/client/desktop/ui/actions/actions.h>
#include <server/server_storage_manager.h>
#include <ui/delegates/recording_stats_item_delegate.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/models/recording_stats_adapter.h>
#include <ui/models/recording_stats_model.h>
#include <ui/utils/table_export_helper.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/workbench/workbench_context.h>
#include <utils/common/event_processors.h>

using namespace nx::vms::client::desktop;
using namespace nx::vms::client::desktop::ui;
using namespace nx::core;

namespace {

using namespace std::chrono_literals;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::minutes;
using std::chrono::hours;

const qint64 kDefaultBitrateAveragingPeriod = milliseconds(5min).count();

// TODO: #sivanov #vkutin Refactor to use HumanReadable helper class.
const qint64 kBytesInGB = 1024ll * 1024 * 1024;
const qint64 kBytesInTB = 1024ll * kBytesInGB;

const int kTableRowHeight = 24;
const int kMinimumColumnWidth = 110;

static const QColor kLight16Color = "#698796";
static const nx::vms::client::core::SvgIconColorer::IconSubstitutions kIconSubstitutions = {
    {QIcon::Normal, {{kLight16Color, "light16"}}},
    {QIcon::Active, {{kLight16Color, "light17"}}},
};

// TODO: #rvasilenko refactor all algorithms working with kExtraDataBase to STL
const std::array<qint64, 5> kExtraDataBase =
{
    10 * kBytesInGB,
    1 * kBytesInTB,
    10 * kBytesInTB,
    100 * kBytesInTB,
    1000 * kBytesInTB
};

const int kTicksPerInterval = 100;

} // namespace

QnStorageAnalyticsWidget::QnStorageAnalyticsWidget(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new ::Ui::StorageAnalyticsWidget),
    m_model(new QnRecordingStatsModel(false, this)),
    m_forecastModel(new QnRecordingStatsModel(true, this)),
    m_selectAllAction(new QAction(tr("Select All"), this)),
    m_exportAction(new QAction(tr("Export Selection to File..."), this)),
    m_clipboardAction(new QAction(tr("Copy Selection to Clipboard"), this))
{
    ui->setupUi(this);

    m_hintButton = new HintButton(tr("Forecast available only for cameras with enabled recording."),
        ui->forecastTotalsTable);

    setWarningStyle(ui->warningLabel);
    ui->extraSpaceSlider->setProperty(nx::style::Properties::kSliderFeatures,
        static_cast<int>(nx::style::SliderFeature::FillingUp));

    auto refreshButton = new QPushButton(ui->tabWidget);
    refreshButton->setFlat(true);
    refreshButton->setText(tr("Refresh"));
    refreshButton->setIcon(qnSkin->icon("text_buttons/reload_20.svg", kIconSubstitutions));
    refreshButton->resize(refreshButton->sizeHint());

    anchorWidgetToParent(refreshButton, Qt::RightEdge | Qt::TopEdge);
    m_hintAnchor = anchorWidgetToParent(m_hintButton, Qt::LeftEdge | Qt::TopEdge);

    ui->tabWidget->tabBar()->setProperty(nx::style::Properties::kTabShape,
        static_cast<int>(nx::style::TabShape::Compact));

    setupTableView(ui->statsTable, ui->statsTotalsTable, m_model);
    setupTableView(ui->forecastTable, ui->forecastTotalsTable, m_forecastModel);

    ui->averagingPeriodCombobox->addItem(tr("Last 5 minutes"),
        QVariant::fromValue<qint64>(milliseconds(5min).count()));
    ui->averagingPeriodCombobox->addItem(tr("Last 60 minutes"),
        QVariant::fromValue<qint64>(milliseconds(1h).count()));
    ui->averagingPeriodCombobox->addItem(tr("Last 24 hours"),
        QVariant::fromValue<qint64>(milliseconds(24h).count()));
    ui->averagingPeriodCombobox->addItem(tr("Longest period available"), 0);
    ui->averagingPeriodCombobox->setCurrentIndex(2); // 24h.

    connect(ui->tabWidget, &QTabWidget::currentChanged, //< Inactive tab may be not updated.
        this, [this](int) { atPageChanged(); });
    connect(ui->averagingPeriodCombobox, qOverload<int>(&QComboBox::currentIndexChanged),
        this, [this](int) { atAveragingPeriodChanged(); });

    connect(m_clipboardAction, &QAction::triggered,
        this, &QnStorageAnalyticsWidget::atClipboardAction_triggered);

    connect(m_exportAction, &QAction::triggered,
        this, &QnStorageAnalyticsWidget::atExportAction_triggered);

    connect(refreshButton, &QAbstractButton::clicked,
        this, [this] { clearCache(); updateDataFromServer(); });

    connect(ui->extraSpaceSlider, &QSlider::valueChanged,
        this, &QnStorageAnalyticsWidget::atForecastParamsChanged);

    connect(ui->extraSizeSpinBox, QnSpinboxDoubleValueChanged,
        this, &QnStorageAnalyticsWidget::atForecastParamsChanged);

    connect(m_selectAllAction, &QAction::triggered,
        this, [this]() { currentTable()->selectAll(); });

    auto storageManager = qnServerStorageManager;
    connect(storageManager, &QnServerStorageManager::storageSpaceRecieved,
        this, &QnStorageAnalyticsWidget::atReceivedSpaceInfo);

    setHelpTopic(this, HelpTopic::Id::ServerSettings_StorageAnalitycs);

    // TODO: #sivanov Use HumanReadable texts instead.
    ui->extraSizeSpinBox->setSuffix(' ' + tr("TB", "TB - terabytes"));
    ui->maxSizeLabel->setText(tr("%n TB", "TB - terabytes",
        qRound(ui->extraSizeSpinBox->maximum())));

    m_statsTableScrollbar = new SnappedScrollBar(this);
    m_statsTableScrollbar->setUseMaximumSpace(true);
    ui->statsTable->setVerticalScrollBar(m_statsTableScrollbar->proxyScrollBar());
    m_forecastTableScrollbar = new SnappedScrollBar(this);
    m_forecastTableScrollbar->setUseMaximumSpace(true);
    ui->forecastTable->setVerticalScrollBar(m_forecastTableScrollbar->proxyScrollBar());

    atPageChanged();
}

QnStorageAnalyticsWidget::~QnStorageAnalyticsWidget()
{
}

TableView* QnStorageAnalyticsWidget::currentTable() const
{
    return ui->tabWidget->currentWidget() == ui->statsTab
        ? ui->statsTable
        : ui->forecastTable;
}

TableView* QnStorageAnalyticsWidget::currentTotalsTable() const
{
    return ui->tabWidget->currentWidget() == ui->statsTab
        ? ui->statsTotalsTable
        : ui->forecastTotalsTable;
}

qint64 QnStorageAnalyticsWidget::currentForecastAveragingPeriod()
{
    return ui->averagingPeriodCombobox->currentData().toLongLong();
}

void QnStorageAnalyticsWidget::setupTableView(TableView* table, TableView* totalsTable,
    QAbstractItemModel* model)
{
    // Setting main table.
    auto sortModel = new QnSortedRecordingStatsModel(this);
    sortModel->setSourceModel(model);
    table->setModel(sortModel);
    table->setItemDelegate(new QnRecordingStatsItemDelegate(this));

    table->verticalHeader()->setMinimumSectionSize(kTableRowHeight);
    table->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

    QHeaderView* header = new QHeaderView(Qt::Horizontal, this);
    table->setHorizontalHeader(header);

    header->setSectionsClickable(true);
    header->setMinimumSectionSize(kMinimumColumnWidth);
    header->setSectionResizeMode(QHeaderView::Fixed);
    header->setSectionResizeMode(QnRecordingStatsModel::CameraNameColumn, QHeaderView::Stretch);
    header->setDefaultAlignment(Qt::AlignLeft);
    header->setSortIndicatorShown(true);

    m_selectAllAction->setShortcut(QKeySequence::SelectAll);
    m_clipboardAction->setShortcut(QKeySequence::Copy);

    installEventHandler(table->viewport(), QEvent::MouseButtonRelease,
        this, &QnStorageAnalyticsWidget::atMouseButtonRelease);

    table->addAction(m_clipboardAction);
    table->addAction(m_exportAction);

    connect(table, &QTableView::customContextMenuRequested,
        this, &QnStorageAnalyticsWidget::atEventsGrid_customContextMenuRequested);

    // Setting totals table.
    auto totalsModel = new QnTotalRecordingStatsModel(this);
    totalsModel->setSourceModel(model);
    totalsTable->setModel(totalsModel);
    totalsTable->setItemDelegate(new QnRecordingStatsItemDelegate(this));
    totalsTable->verticalHeader()->setMinimumSectionSize(kTableRowHeight);
    totalsTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    totalsTable->setProperty(nx::style::Properties::kSuppressHoverPropery, true);
}

QnMediaServerResourcePtr QnStorageAnalyticsWidget::server() const
{
    return m_server;
}

void QnStorageAnalyticsWidget::setServer(const QnMediaServerResourcePtr& server)
{
    if (m_server == server)
        return;

    m_server = server;

    clearCache();
    m_dirty = true;
}

void QnStorageAnalyticsWidget::loadDataToUi()
{
    ui->tabWidget->setCurrentWidget(ui->statsTab);

    if(isVisible() && m_dirty)
        updateDataFromServer();
}

void QnStorageAnalyticsWidget::updateDataFromServer()
{
    querySpaceFromServer();
    queryStatsFromServer(kDefaultBitrateAveragingPeriod);
    // Do second stats request if forecast is made for different averaging period.
    if (currentForecastAveragingPeriod() != kDefaultBitrateAveragingPeriod)
        queryStatsFromServer(currentForecastAveragingPeriod());

    // update UI
    if (requestsInProgress())
    {
        ui->statsTable->setEnabled(false);
        ui->statsTotalsTable->setEnabled(false);
        ui->forecastTable->setEnabled(false);
        ui->forecastTotalsTable->setEnabled(false);
        ui->stackedWidget->setCurrentWidget(ui->gridPage);
        setCursor(Qt::BusyCursor);
    }
    else
    {
        processRequestFinished(); // just clear grid
        ui->stackedWidget->setCurrentWidget(ui->warnPage);
    }

    m_dirty = false;
}

void QnStorageAnalyticsWidget::clearCache()
{
    m_availableStorageSpace = 0;
    m_availableStorages.clear();
    m_recordingsStatData.clear();
}

bool QnStorageAnalyticsWidget::requestsInProgress() const
{
    return m_statsRequest[0].handle != -1 || m_statsRequest[1].handle != -1
        || m_spaceRequestHandle != -1;
}

void QnStorageAnalyticsWidget::querySpaceFromServer()
{
    m_availableStorages.clear();

    if (!m_server || m_server->getStatus() != nx::vms::api::ResourceStatus::online)
        return;

    auto storageManager = qnServerStorageManager;
    // If next call fails, it will return 0 meaning "no request".
    m_spaceRequestHandle = storageManager->requestStorageSpace(m_server);
}

void QnStorageAnalyticsWidget::atReceivedSpaceInfo(QnMediaServerResourcePtr server,
    bool success, int handle, const nx::vms::api::StorageSpaceReply& data)
{
    if (server != m_server)
        return;
    if (m_spaceRequestHandle != handle)
        return;

    m_spaceRequestHandle = -1;

    if (success)
        m_availableStorages = data.storages;

    processRequestFinished();
}

void QnStorageAnalyticsWidget::queryStatsFromServer(qint64 bitrateAveragingPeriodMs)
{
    m_recordingsStatData.remove(bitrateAveragingPeriodMs);

    if (!m_server || m_server->getStatus() != nx::vms::api::ResourceStatus::online)
        return;

    const auto index = bitrateAveragingPeriodMs == kDefaultBitrateAveragingPeriod ? 0 : 1;
    // If next call fails, it will return -1 meaning "no request".
    m_statsRequest[index].averagingPeriod = bitrateAveragingPeriodMs;
    auto storageManager = qnServerStorageManager;
    m_statsRequest[index].handle = storageManager->requestRecordingStatistics(
        m_server, bitrateAveragingPeriodMs,
        [tool=QPointer(this)](bool success, int handle, const QnRecordingStatsReply& data)
        {
            if (tool)
                tool->atReceivedStats(success, handle, data);
        });
}

void QnStorageAnalyticsWidget::atReceivedStats(
    bool success, int handle, const QnRecordingStatsReply& data)
{
    StatsRequest* request;
    // There could be 2 simultaneous requests, select the correct one.
    if (m_statsRequest[0].handle == handle)
        request = &(m_statsRequest[0]);
    else if (m_statsRequest[1].handle == handle)
        request = &(m_statsRequest[1]);
    else
        return;

    request->handle = -1;

    if (success)
    {
        m_recordingsStatData[request->averagingPeriod] = {};

        for (const auto& value: data)
            m_recordingsStatData[request->averagingPeriod] << value;
    }

    processRequestFinished();
}

void QnStorageAnalyticsWidget::processRequestFinished()
{
    if (requestsInProgress())
        return;

    // Calculate available space on storages.
    m_availableStorageSpace = QnRecordingStats::calculateAvailableSpace(
        m_recordingsStatData[kDefaultBitrateAveragingPeriod], m_availableStorages);

    // Prepare and set model for "Storage Analytics" page.
    m_model->setModelData(QnRecordingStats::transformStatsToModelData(
        m_recordingsStatData[kDefaultBitrateAveragingPeriod], m_server, resourcePool()));

    ui->statsTable->setEnabled(true);
    ui->statsTotalsTable->setEnabled(true);
    ui->forecastTable->setEnabled(true);
    ui->forecastTotalsTable->setEnabled(true);

    atForecastParamsChanged(); //< Forecast model is set here.

    setCursor(Qt::ArrowCursor);
}

void QnStorageAnalyticsWidget::atEventsGrid_customContextMenuRequested(const QPoint&)
{
    auto table = currentTable();

    QScopedPointer<QMenu> menu(new QMenu(this));

    QModelIndexList selectedIndexes = table->selectionModel()->selectedRows(); //< Only 1 index for row.
    QnResourceList selectedResources;
    if (!selectedIndexes.empty())
    {
        for (auto index: selectedIndexes)
        {
            if (auto resource = table->model()->data(index, Qn::ResourceRole).value<QnResourcePtr>())
                selectedResources.append(resource);
        }
    }

    if (!selectedResources.empty())
    {
        action::Parameters parameters(selectedResources);
        parameters.setArgument(Qn::NodeTypeRole, ResourceTree::NodeType::resource);
        auto manager = context()->menu();
        menu.reset(manager->newMenu(
            action::TreeScope,
            this,
            parameters,
            action::Manager::CreationOptions(),
            {action::IDType::CameraSettingsAction}));

        for (auto action: menu->actions())
            action->setShortcut({});
    }

    if (!menu->actions().empty())
        menu->addSeparator();

    m_clipboardAction->setEnabled(table->selectionModel()->hasSelection());
    m_exportAction->setEnabled(table->selectionModel()->hasSelection());

    menu->addAction(m_selectAllAction);
    menu->addAction(m_exportAction);
    menu->addAction(m_clipboardAction);

    QnHiDpiWorkarounds::showMenu(menu.data(), QCursor::pos());
}

void QnStorageAnalyticsWidget::atExportAction_triggered()
{
    const auto model = currentTable()->model();
    const auto totalsModel = currentTotalsTable()->model();
    const auto selectionModel = currentTable()->selectionModel();
    const bool isAllRowsSelected = selectionModel->selectedRows().size() == model->rowCount();

    if (isAllRowsSelected)
    {
        QnTableExportCompositeModel compositeModel({model, totalsModel});
        QnTableExportHelper::exportToFile(
            &compositeModel,
            QnTableExportHelper::getAllIndexes(&compositeModel),
            this,
            tr("Export selected events to file"));
    }
    else
    {
        QnTableExportHelper::exportToFile(
            currentTable()->model(),
            currentTable()->selectionModel()->selectedIndexes(),
            this,
            tr("Export selected events to file"));
    }
}

void QnStorageAnalyticsWidget::atClipboardAction_triggered()
{
    const auto model = currentTable()->model();
    const auto totalsModel = currentTotalsTable()->model();
    const auto selectionModel = currentTable()->selectionModel();
    const bool isAllRowsSelected = selectionModel->selectedRows().size() == model->rowCount();

    if (isAllRowsSelected)
    {
        QnTableExportCompositeModel compositeModel({model, totalsModel});
        QnTableExportHelper::copyToClipboard(
            &compositeModel,
            QnTableExportHelper::getAllIndexes(&compositeModel));
    }
    else
    {
        QnTableExportHelper::copyToClipboard(
            currentTable()->model(),
            currentTable()->selectionModel()->selectedIndexes());
    }
}

void QnStorageAnalyticsWidget::atMouseButtonRelease(QObject*, QEvent* event)
{
    QMouseEvent* me = dynamic_cast<QMouseEvent*> (event);
    m_lastMouseButton = me->button();
}

qint64 QnStorageAnalyticsWidget::sliderPositionToBytes(int value) const
{
    if (value == 0)
        return 0;

    const int idx = value / kTicksPerInterval;
    if (idx >= (qint64) kExtraDataBase.size() - 1)
        return (qint64) kExtraDataBase.back();

    const qint64 k1 = kExtraDataBase[idx];
    const qint64 k2 = kExtraDataBase[idx+1];
    const int intervalTicks = value % kTicksPerInterval;

    const qint64 step = (k2 - k1) / kTicksPerInterval;
    return k1 + step * intervalTicks;
}

int QnStorageAnalyticsWidget::bytesToSliderPosition(qint64 value) const
{
    int idx = 0;
    for (; idx < (qint64) kExtraDataBase.size() - 1; ++idx)
    {
        if (kExtraDataBase[idx+1] >= value)
            break;
    }
    qint64 step = (kExtraDataBase[idx+1] - kExtraDataBase[idx]) / kTicksPerInterval;
    value -= kExtraDataBase[idx];
    return idx * kTicksPerInterval + value / step;
}

void QnStorageAnalyticsWidget::atForecastParamsChanged()
{
    if (m_updating)
        return;

    QScopedValueRollback<bool> guard(m_updating, true);

    if (!ui->forecastTable->isEnabled())
        return;

    ui->forecastTable->setEnabled(false);
    ui->forecastTotalsTable->setEnabled(false);

    qint64 additionalSpace = 0;
    if (sender() == ui->extraSpaceSlider)
    {
        additionalSpace = sliderPositionToBytes(ui->extraSpaceSlider->value());
        ui->extraSizeSpinBox->setValue(additionalSpace / (qreal) kBytesInTB);
    }
    else
    {
        additionalSpace = ui->extraSizeSpinBox->value() * kBytesInTB;
        ui->extraSpaceSlider->setValue(bytesToSliderPosition(additionalSpace));
    }

    // Do forecast based on m_availableStorageSpace + additionalSpace.
    m_forecastModel->setModelData(
        QnRecordingStats::forecastFromStatsToModel(
            m_recordingsStatData[currentForecastAveragingPeriod()],
            additionalSpace + m_availableStorageSpace,
            m_server, resourcePool()));

    ui->forecastTable->setEnabled(true);
    ui->forecastTotalsTable->setEnabled(true);
    updateTotalTablesGeometry();
}

void QnStorageAnalyticsWidget::atAveragingPeriodChanged()
{
    // Request data from server if not in cache.
    if (!m_recordingsStatData.contains(currentForecastAveragingPeriod()))
        updateDataFromServer();
    else
        atForecastParamsChanged();
}

void QnStorageAnalyticsWidget::atPageChanged()
{
    // Show right scrollbar.
    m_statsTableScrollbar->setVisible(currentTable() == ui->statsTable
        && m_statsTableScrollbar->maximum() > m_statsTableScrollbar->minimum());
    m_forecastTableScrollbar->setVisible(currentTable() == ui->forecastTable
        && m_forecastTableScrollbar->maximum() > m_forecastTableScrollbar->minimum());

    updateTotalTablesGeometry();
}

void QnStorageAnalyticsWidget::updateTotalTablesGeometry()
{
    // Stats table.
    for (int i = 0; i < m_model->columnCount(); i++)
        ui->statsTotalsTable->setColumnWidth(i, ui->statsTable->columnWidth(i));
    // Forecast table.
    for (int i = 0; i < m_forecastModel->columnCount(); i++)
        ui->forecastTotalsTable->setColumnWidth(i, ui->forecastTable->columnWidth(i));
    positionHintButton();
}

void QnStorageAnalyticsWidget::positionHintButton()
{
    const auto cellSize = ui->forecastTotalsTable-> sizeHintForIndex(
        ui->forecastTotalsTable->model()->index(0, 0));

    if (cellSize.isValid())
    {
        // Top = 12 is magic, but looks good.
        m_hintAnchor->setMargins(cellSize.width(), 12, 0, 0);
        m_hintButton->setVisible(true);
    }
    else
    {
        m_hintButton->setVisible(false);
    }
}

void QnStorageAnalyticsWidget::resizeEvent(QResizeEvent*)
{
    // We assume that tables have equal width when visible.
    // However, only current (visible) table has correct width on the first show.
    const int tableWidth = currentTable()->width();

    // Setup table column widths (40%/20%/20%/20% and 40%/30%/30%).
    // First (0) column will stretch.
    ui->statsTable->setColumnWidth(1, (2 * tableWidth) / 10);
    ui->statsTable->setColumnWidth(2, (2 * tableWidth) / 10);
    ui->statsTable->setColumnWidth(3, (2 * tableWidth) / 10);

    // First column will stretch.
    ui->forecastTable->setColumnWidth(1, (3 * tableWidth) / 10);
    ui->forecastTable->setColumnWidth(2, (3 * tableWidth) / 10);

    updateTotalTablesGeometry();
}

void QnStorageAnalyticsWidget::showEvent(QShowEvent*)
{
    resizeEvent(nullptr); //< We know true widget geometry only at this point.
    if (m_dirty) //< Update info if it was previously requested.
        updateDataFromServer();
}
