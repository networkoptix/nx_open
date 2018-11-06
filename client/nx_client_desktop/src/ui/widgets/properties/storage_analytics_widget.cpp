#include "storage_analytics_widget.h"
#include "ui_storage_analytics_widget.h"

#include <set>
#include <chrono>

#include <QtCore/QMimeData>
#include <QtGui/QClipboard>
#include <QtWidgets/QMenu>
#include <QtGui/QShowEvent>

#include <client/client_color_types.h>
#include <client/client_globals.h>
#include <client/client_settings.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

#include <nx/client/desktop/ui/actions/action_manager.h>
#include <nx/client/desktop/ui/actions/actions.h>
#include <nx/client/desktop/common/utils/widget_anchor.h>
#include <nx/client/desktop/resource_views/data/node_type.h>
#include <ui/customization/customized.h>
#include <ui/delegates/recording_stats_item_delegate.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/models/recording_stats_model.h>
#include <ui/style/skin.h>
#include <ui/style/custom_style.h>
#include <ui/utils/table_export_helper.h>
#include <nx/client/desktop/common/widgets/dropdown_button.h>
#include <ui/workbench/workbench_context.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/workaround/hidpi_workarounds.h>

#include <utils/common/event_processors.h>

using namespace nx::client::desktop;
using namespace nx::client::desktop::ui;

namespace {

using namespace std::chrono_literals;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::minutes;
using std::chrono::hours;

auto days(int count)
{
    return hours(count * 24);
}

const qint64 kDefaultBitrateAveragingPeriod = milliseconds(5min).count();

// TODO: #GDM #3.1 move out strings and logic to separate class (string.h:bytesToString)
const qint64 kBytesInGB = 1024ll * 1024 * 1024;
const qint64 kBytesInTB = 1024ll * kBytesInGB;
const qint64 kFinalStepSeconds = 1000000000ll * 10;

const int kTableRowHeight = 24;
const int kMinimumColumnWidth = 110;

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
    ui(new Ui::StorageAnalyticsWidget),
    m_model(new QnRecordingStatsModel(false, this)),
    m_forecastModel(new QnRecordingStatsModel(true, this)),
    m_selectAllAction(new QAction(tr("Select All"), this)),
    m_exportAction(new QAction(tr("Export Selection to File..."), this)),
    m_clipboardAction(new QAction(tr("Copy Selection to Clipboard"), this))
{
    ui->setupUi(this);
    setWarningStyle(ui->warningLabel);
    ui->extraSpaceSlider->setProperty(style::Properties::kSliderFeatures,
        static_cast<int>(style::SliderFeature::FillingUp));

    auto refreshButton = new QPushButton(ui->tabWidget);
    refreshButton->setFlat(true);
    refreshButton->setText(tr("Refresh"));
    refreshButton->setIcon(qnSkin->icon(lit("text_buttons/refresh.png")));
    refreshButton->resize(refreshButton->sizeHint());

    anchorWidgetToParent(refreshButton, Qt::RightEdge | Qt::TopEdge);

    ui->tabWidget->tabBar()->setProperty(style::Properties::kTabShape,
        static_cast<int>(style::TabShape::Compact));

    setupTableView(ui->statsTable, m_model);
    setupTableView(ui->forecastTable, m_forecastModel);

    ui->averagingPeriodCombobox->addItem(tr("The last minute"), milliseconds(1min).count());
    ui->averagingPeriodCombobox->addItem(tr("The last 5 minutes"), milliseconds(5min).count());
    ui->averagingPeriodCombobox->addItem(tr("The last hour"), milliseconds(1h).count());
    ui->averagingPeriodCombobox->addItem(tr("The last 24 hours"), milliseconds(24h).count());
    ui->averagingPeriodCombobox->addItem(tr("All recorded data"), 0);
    ui->averagingPeriodCombobox->setCurrentIndex(1); // 5 min.

    connect(ui->averagingPeriodCombobox, qOverload<int>(&QComboBox::currentIndexChanged),
        this, [this](int) { at_averagingPeriodChanged(); });

    connect(m_clipboardAction, &QAction::triggered,
        this, &QnStorageAnalyticsWidget::at_clipboardAction_triggered);

    connect(m_exportAction, &QAction::triggered,
        this, &QnStorageAnalyticsWidget::at_exportAction_triggered);

    connect(refreshButton, &QAbstractButton::clicked,
        this, [this] { clearCache(); updateDataFromServer(); });

    connect(ui->extraSpaceSlider, &QSlider::valueChanged,
        this, &QnStorageAnalyticsWidget::at_forecastParamsChanged);

    connect(ui->extraSizeSpinBox, QnDoubleSpinBoxValueChanged,
        this, &QnStorageAnalyticsWidget::at_forecastParamsChanged);

    connect(m_selectAllAction, &QAction::triggered,
        this, [this]() { currentTable()->selectAll(); });

    setHelpTopic(this, Qn::ServerSettings_StorageAnalitycs_Help);

    // TODO: #GDM move to std texts
    ui->extraSizeSpinBox->setSuffix(L' ' + tr("TB", "TB - terabytes"));
    ui->maxSizeLabel->setText(tr("%n TB", "TB - terabytes",
        qRound(ui->extraSizeSpinBox->maximum())));
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

qint64 QnStorageAnalyticsWidget::currentForecastAveragingPeriod()
{
    return ui->averagingPeriodCombobox->currentData().toLongLong();
}

void QnStorageAnalyticsWidget::setupTableView(TableView* table, QAbstractItemModel* model)
{
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
    header->setSortIndicatorShown(true);

    m_selectAllAction->setShortcut(QKeySequence::SelectAll);
    m_clipboardAction->setShortcut(QKeySequence::Copy);

    installEventHandler(table->viewport(), QEvent::MouseButtonRelease,
        this, &QnStorageAnalyticsWidget::at_mouseButtonRelease);

    table->addAction(m_clipboardAction);
    table->addAction(m_exportAction);

    connect(table, &QTableView::customContextMenuRequested, this, &QnStorageAnalyticsWidget::at_eventsGrid_customContextMenuRequested);
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

void QnStorageAnalyticsWidget::applyChanges()
{
    /* This widget is read-only. */
}

bool QnStorageAnalyticsWidget::hasChanges() const
{
    return false;
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
        ui->statsTable->setDisabled(true);
        ui->forecastTable->setDisabled(true);
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
    m_availableStorages.clear();
    m_recordingsStatData.clear();
}

void QnStorageAnalyticsWidget::querySpaceFromServer()
{
    m_availableStorages.clear();

    if (!m_server || m_server->getStatus() != Qn::Online)
        return;

    // If next call fails, it will return -1 meaning "no request".
    m_spaceRequestHandle = m_server->apiConnection()->getStorageSpaceAsync(false,
        this, SLOT(at_receivedSpaceInfo(int, const QnStorageSpaceReply&, int)));
}

void QnStorageAnalyticsWidget::queryStatsFromServer(qint64 bitrateAveragingPeriodMs)
{
    m_recordingsStatData.remove(bitrateAveragingPeriodMs);

    if (!m_server || m_server->getStatus() != Qn::Online)
        return;

    const auto index = bitrateAveragingPeriodMs == kDefaultBitrateAveragingPeriod ? 0 : 1;
    // If next call fails, it will return -1 meaning "no request".
    m_statsRequest[index].averagingPeriod = bitrateAveragingPeriodMs;
    m_statsRequest[index].handle = m_server->apiConnection()->getRecordingStatisticsAsync(
        bitrateAveragingPeriodMs, this, SLOT(at_receivedStats(int, const QnRecordingStatsReply&, int)));
}

bool QnStorageAnalyticsWidget::requestsInProgress() const
{
    return m_statsRequest[0].handle != -1 || m_statsRequest[1].handle != -1
        || m_spaceRequestHandle != -1;
}

void QnStorageAnalyticsWidget::at_receivedStats(int status, const QnRecordingStatsReply& data,
    int requestNum)
{
    StatsRequest*  request;
    // There could be 2 simultaneous requests, select the correct one.
    if (m_statsRequest[0].handle == requestNum)
        request = &(m_statsRequest[0]);
    else if (m_statsRequest[1].handle == requestNum)
        request = &(m_statsRequest[1]);
    else
        return;

    request->handle = -1;

    if (status == 0)
    {
        // Create map entry because we got the reply.
        if(!m_recordingsStatData.contains(request->averagingPeriod))
            m_recordingsStatData[request->averagingPeriod] = QnRecordingStatsReply();

        for (const auto& value: data)
            m_recordingsStatData[request->averagingPeriod] << value;
    }

    processRequestFinished();
}

void QnStorageAnalyticsWidget::at_receivedSpaceInfo(int status, const QnStorageSpaceReply& data,
    int requestNum)
{
    if (m_spaceRequestHandle != requestNum)
        return;

    m_spaceRequestHandle = -1;

    if (status == 0)
    {
        for (const auto& storage : data.storages)
            m_availableStorages << storage;
    }

    processRequestFinished();
}

QnRecordingStatsReply QnStorageAnalyticsWidget::filterStatsReply(const QnRecordingStatsReply& cameraStats)
{
    QnRecordingStatsReply visibleCameras;
    QnRecordingStatsReply hiddenCameras;

    for (const auto& camera : cameraStats)
    {
        const auto& cam = resourcePool()->getResourceByUniqueId(camera.uniqueId);
        if (cam && cam->getParentId() == m_server->getId())
            visibleCameras << camera;
        else
            hiddenCameras << camera; // hide all cameras which belong to another server
    }

    if (!hiddenCameras.isEmpty())
    {
        QnCamRecordingStatsData extraRecord; // data occuped by foreign cameras and cameras missed at resource pool
        extraRecord.uniqueId = QnSortedRecordingStatsModel::kForeignCameras;
        for (const auto& hiddenRecord: hiddenCameras)
            extraRecord.recordedBytes += hiddenRecord.recordedBytes;

        visibleCameras << extraRecord;
    }
    return visibleCameras;
}

void QnStorageAnalyticsWidget::processRequestFinished()
{
    if (requestsInProgress())
        return;

    m_model->setModelData(filterStatsReply(m_recordingsStatData[kDefaultBitrateAveragingPeriod]));
    m_forecastModel->setModelData(filterStatsReply(m_recordingsStatData[currentForecastAveragingPeriod()]));

    ui->statsTable->setDisabled(false);
    ui->forecastTable->setDisabled(false);
    at_forecastParamsChanged();
    setCursor(Qt::ArrowCursor);
}

void QnStorageAnalyticsWidget::at_eventsGrid_customContextMenuRequested(const QPoint&)
{
    auto table = currentTable();

    QScopedPointer<QMenu> menu;
    QModelIndex idx = table->currentIndex();
    if (idx.isValid())
    {
        QnResourcePtr resource = table->model()->data(idx, Qn::ResourceRole).value<QnResourcePtr>();
        auto manager = context()->menu();

        if (resource)
        {
            action::Parameters parameters(resource);
            parameters.setArgument(Qn::NodeTypeRole, ResourceTreeNodeType::resource);

            menu.reset(manager->newMenu(action::TreeScope, nullptr, parameters));
            foreach(QAction* action, menu->actions())
                action->setShortcut(QKeySequence());
        }
    }

    if (menu)
        menu->addSeparator();
    else
        menu.reset(new QMenu());

    m_clipboardAction->setEnabled(table->selectionModel()->hasSelection());
    m_exportAction->setEnabled(table->selectionModel()->hasSelection());

    menu->addSeparator();

    menu->addAction(m_selectAllAction);
    menu->addAction(m_exportAction);
    menu->addAction(m_clipboardAction);

    QnHiDpiWorkarounds::showMenu(menu.data(), QCursor::pos());
}

void QnStorageAnalyticsWidget::at_exportAction_triggered()
{
    QnTableExportHelper::exportToFile(currentTable(), true, this, tr("Export selected events to file"));
}

void QnStorageAnalyticsWidget::at_clipboardAction_triggered()
{
    QnTableExportHelper::copyToClipboard(currentTable());
}

void QnStorageAnalyticsWidget::at_mouseButtonRelease(QObject*, QEvent* event)
{
    QMouseEvent* me = dynamic_cast<QMouseEvent*> (event);
    m_lastMouseButton = me->button();
}

qint64 QnStorageAnalyticsWidget::sliderPositionToBytes(int value) const
{
    if (value == 0)
        return 0;

    int idx = value / kTicksPerInterval;
    if (idx >= (qint64) kExtraDataBase.size() - 1)
        return (qint64) kExtraDataBase.back();

    qint64 k1 = kExtraDataBase[idx];
    qint64 k2 = kExtraDataBase[idx+1];
    int intervalTicks = value % kTicksPerInterval;

    qint64 step = (k2 - k1) / kTicksPerInterval;
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

void QnStorageAnalyticsWidget::at_forecastParamsChanged()
{
    if (m_updating)
        return;

    QScopedValueRollback<bool> guard(m_updating, true);

    if (!ui->forecastTable->isEnabled())
        return;

    ui->forecastTable->setEnabled(false);

    qint64 forecastedSize = 0;
    if (sender() == ui->extraSpaceSlider)
    {
        forecastedSize = sliderPositionToBytes(ui->extraSpaceSlider->value());
        ui->extraSizeSpinBox->setValue(forecastedSize / (qreal) kBytesInTB);
    }
    else
    {
        forecastedSize = ui->extraSizeSpinBox->value() * kBytesInTB;
        ui->extraSpaceSlider->setValue(bytesToSliderPosition(forecastedSize));
    }

    m_forecastModel->setModelData(getForecastData(forecastedSize));

    ui->forecastTable->setEnabled(true);
}

void QnStorageAnalyticsWidget::at_averagingPeriodChanged()
{
    // Request data from server if not in cache.
    if (!m_recordingsStatData.contains(currentForecastAveragingPeriod()))
        updateDataFromServer();
    else
    {
        m_forecastModel->setModelData(filterStatsReply(m_recordingsStatData[currentForecastAveragingPeriod()]));
        at_forecastParamsChanged();
    }
}

QnRecordingStatsReply QnStorageAnalyticsWidget::getForecastData(qint64 extraSizeBytes)
{
    const QnRecordingStatsReply modelData = m_forecastModel->modelData();
    ForecastData forecastData;

    // 1. collect camera related forecast params
    bool hasExpaned = false;
    for (const auto& cameraStats: modelData)
    {
        ForecastDataPerCamera cameraForecast;

        QnSecurityCamResourcePtr camRes = resourcePool()->getResourceByUniqueId<QnSecurityCamResource>(cameraStats.uniqueId);
        if (camRes)
        {
            cameraForecast.expand = camRes->isLicenseUsed();
            cameraForecast.expand &= (camRes->getStatus() == Qn::Online || camRes->getStatus() == Qn::Recording);
            cameraForecast.expand &= cameraStats.archiveDurationSecs > 0 && cameraStats.recordedBytes > 0;
            cameraForecast.minDays = qMax(0, camRes->minDays());
            cameraForecast.maxDays = qMax(0, camRes->maxDays());
            cameraForecast.byterate = qMax(1ll, cameraStats.averageBitrate);

            if (cameraForecast.expand)
                hasExpaned = true;
        }

        cameraForecast.stats.uniqueId = cameraStats.uniqueId;
        cameraForecast.stats.averageBitrate = cameraStats.averageBitrate;
        forecastData.cameras.push_back(std::move(cameraForecast));
        //forecastData.totalSpace += cameraStats.recordedBytes; // 2.1 add current archive space

        if (cameraStats.uniqueId == QnSortedRecordingStatsModel::kForeignCameras)
        {
            forecastData.totalSpace += cameraStats.recordedBytes; //<< there are no storages for virtual camera called 'foreign cameras'
        }
        else
        {
            for (auto itr = cameraStats.recordedBytesPerStorage.begin(); itr != cameraStats.recordedBytesPerStorage.end(); ++itr)
            {
                for (const auto& storageSpaceData: m_availableStorages)
                {
                    if (storageSpaceData.storageId == itr.key() && storageSpaceData.isUsedForWriting && storageSpaceData.isWritable)
                        forecastData.totalSpace += itr.value();
                }
            }
        }
    }

    if (!hasExpaned)
        return modelData; // no recording cameras at all. Do not forecast anything

    // 2.1 add free storage space
    for (const auto& storageData: m_availableStorages)
    {
        QnResourcePtr storageRes = resourcePool()->getResourceById(storageData.storageId);
        if (!storageRes)
            continue;

        if (storageData.isUsedForWriting && storageData.isWritable && !storageData.isBackup)
            forecastData.totalSpace += qMax(0ll, storageData.freeSpace - storageData.reservedSpace);
    }

    // 2.2 add user extra data
    forecastData.totalSpace += extraSizeBytes;

    return doForecast(std::move(forecastData));
}

QnRecordingStatsReply QnStorageAnalyticsWidget::doForecast(ForecastData forecastData)
{
    std::set<seconds> steps; // select possible values for minDays variable
    for (const auto& camera: forecastData.cameras)
    {
        if (camera.minDays > 0)
            steps.insert(days(camera.minDays));
    }
    for (const auto& seconds: steps)
    {
        spendData(forecastData, seconds.count(),
            [seconds](const ForecastDataPerCamera& stats)
            {
                return stats.expand && days(stats.minDays) >= seconds;
            });
    }

    for (const auto& camera: forecastData.cameras)
    {
        if (camera.maxDays > 0)
            steps.insert(days(camera.maxDays));
    }

    steps.insert(seconds(kFinalStepSeconds)); // final step for all cameras

    for (const auto& seconds: steps)
    {
        spendData(forecastData, seconds.count(),
            [seconds](const ForecastDataPerCamera& stats)
            {
                return stats.expand && (days(stats.maxDays) >= seconds || stats.maxDays == 0);
            });
    }

    QnRecordingStatsReply result;
    for (auto& value: forecastData.cameras)
    {
        value.stats.recordedBytes = value.byterate * value.stats.archiveDurationSecs;
        result << value.stats;
    }

    return result;
}

void QnStorageAnalyticsWidget::spendData(ForecastData& forecastData, qint64 needSeconds,
    std::function<bool (const ForecastDataPerCamera& stats)> predicate)
{
    qint64 lackingBytes = 0;
    for (ForecastDataPerCamera& cameraForecast: forecastData.cameras)
    {
        if (!predicate(cameraForecast))
            continue;

        const qint64 lackingSeconds = qMax(0ll, needSeconds - cameraForecast.stats.archiveDurationSecs);
        lackingBytes += lackingSeconds * cameraForecast.byterate;
    }

    // we have less bytes left then required
    qreal coeff = 1.0;
    if (lackingBytes > forecastData.totalSpace)
        coeff = forecastData.totalSpace / (qreal) lackingBytes;

    // spend data
    for (ForecastDataPerCamera& cameraForecast: forecastData.cameras)
    {
        if (predicate(cameraForecast))
        {
            cameraForecast.stats.archiveDurationSecs += coeff * (needSeconds
                - cameraForecast.stats.archiveDurationSecs);
        }
    }

    forecastData.totalSpace = qMax(0ll, forecastData.totalSpace - lackingBytes);
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
}

void QnStorageAnalyticsWidget::showEvent(QShowEvent*)
{
    resizeEvent(nullptr); //< We know true widget geometry only at this point.
    if (m_dirty) //< Update info if it was previously requested.
        updateDataFromServer();
}
