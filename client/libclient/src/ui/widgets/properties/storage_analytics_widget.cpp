#include "storage_analytics_widget.h"
#include "ui_storage_analytics_widget.h"

#include <QtCore/QMimeData>
#include <QtGui/QClipboard>
#include <QtWidgets/QMenu>
#include <QtGui/QMouseEvent>
#include <QtGui/QShowEvent>

#include <client/client_globals.h>
#include <client/client_settings.h>

#include <core/resource/camera_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

#include <ui/common/widget_anchor.h>
#include <ui/utils/table_export_helper.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/delegates/resource_item_delegate.h>
#include <ui/dialogs/common/custom_file_dialog.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/style/skin.h>
#include <ui/style/custom_style.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include <ui/workaround/hidpi_workarounds.h>
#include <ui/models/recording_stats_model.h>
#include <ui/actions/action_manager.h>
#include <ui/actions/actions.h>

#include <utils/common/event_processors.h>
#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/scoped_value_rollback.h>
#include <utils/math/color_transformations.h>
#include <utils/common/synctime.h>
#include <set>

namespace {

    const qint64 kSecondsPerHour = 3600;
    const qint64 kSecondsPerDay = kSecondsPerHour * 24;
    const qint64 kBytesInGB = 1024ll * 1024 * 1024;
    const qint64 kBytesInTB = 1024ll * kBytesInGB;
    const qint64 kFinalStepSeconds = 1000000000ll * 10;

    //TODO: #rvasilenko refactor all algorithms working with kExtraDataBase to STL
    const std::array<qint64, 5> kExtraDataBase =
    {
        10 * kBytesInGB,
        1 * kBytesInTB,
        10 * kBytesInTB,
        100 * kBytesInTB,
        1000 * kBytesInTB
    };

    const int kTicksPerInterval = 100;

    class QnRecordingStatsItemDelegate: public QStyledItemDelegate
    {
        typedef QStyledItemDelegate base_type;

    public:
        explicit QnRecordingStatsItemDelegate(QObject* parent = nullptr):
            base_type(parent)
        {
        }

        virtual void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
        {
            NX_ASSERT(index.isValid());

            QStyleOptionViewItem opt = option;
            initStyleOption(&opt, index);

            const QWidget* widget = option.widget;
            QStyle* style = widget ? widget->style() : QApplication::style();

            if (index.column() != QnRecordingStatsModel::CameraNameColumn)
            {
                QString label = opt.text;

                opt.text = QString();
                style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

                // draw chart manually
                QnScopedPainterTransformRollback rollback(painter);
                QnScopedPainterBrushRollback rollback2(painter);

                QPoint shift = opt.rect.topLeft();
                painter->translate(shift);
                opt.rect.translate(-shift);

                //qreal realData = index.data(Qn::RecordingStatChartDataRole).toReal();
                //qreal forecastData = index.data(Qn::RecordingStatForecastDataRole).toReal();
                qreal chartData = index.data(Qn::RecordingStatChartDataRole).toReal();
                QColor chartColor;
                QVariant colorData =  index.data(Qn::RecordingStatChartColorDataRole);
                if (colorData.isValid() && colorData.canConvert<QColor>())
                    chartColor = qvariant_cast<QColor>(colorData);

                opt.rect.adjust(2, 1, -2, -1);

                painter->fillRect(QRect(opt.rect.left() , opt.rect.top(), opt.rect.width() * chartData, opt.rect.height()), chartColor);

                QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, opt.widget);

                painter->setFont(opt.font);
                painter->setPen(opt.palette.color(QPalette::ButtonText));
                painter->drawText(textRect, Qt::AlignRight | Qt::AlignVCenter, label);

            }
            else
            {
                style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);
            }
        }
    };

    class CustomHorizontalHeader: public QHeaderView
    {
        Q_DECLARE_TR_FUNCTIONS(CustomHorizontalHeader)

    private:
        QComboBox* m_comboBox;

    private:
        void updateComboBox()
        {
            QRect rect(sectionViewportPosition(QnRecordingStatsModel::BitrateColumn), 0,
                       sectionSize(QnRecordingStatsModel::BitrateColumn), height());

            m_comboBox->setGeometry(QStyle::alignedRect(
                Qt::LeftToRight, Qt::AlignRight | Qt::AlignVCenter, m_comboBox->minimumSizeHint(), rect));
        }

    public:
        CustomHorizontalHeader(QWidget* parent = nullptr) : QHeaderView(Qt::Horizontal, parent)
        {
            m_comboBox = new QComboBox(this);
            m_comboBox->addItem(tr("5 minutes"), 5 * 60);
            m_comboBox->addItem(tr("Hour"),  60 * 60);
            m_comboBox->addItem(tr("Day"),   3600 * 24);
            m_comboBox->addItem(tr("Week"),  3600 * 24 * 7);
            m_comboBox->addItem(tr("Month"), 3600 * 24 * 30);
            m_comboBox->addItem(tr("All Data"), 0);
            m_comboBox->setCurrentIndex(m_comboBox->count()-1);

            connect(this, &QHeaderView::sectionResized, this, [this]() { updateComboBox(); }, Qt::DirectConnection);
            connect(this, &QHeaderView::sectionResized, this, [this]() { updateComboBox(); }, Qt::QueuedConnection);
        }

        virtual void showEvent(QShowEvent *e) override
        {
            QHeaderView::showEvent(e);
            updateComboBox();
            m_comboBox->show();
        }

        QComboBox* comboBox() const
        {
            return m_comboBox;
        }

        int durationForBitrate() const
        {
            return m_comboBox->itemData(m_comboBox->currentIndex()).toInt();
        }
    };

    const int kMinimumSectionSize = 200;
}

QnStorageAnalyticsWidget::QnStorageAnalyticsWidget(QWidget* parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::StorageAnalyticsWidget),
    m_server(),
    m_model(new QnRecordingStatsModel(false, this)),
    m_forecastModel(new QnRecordingStatsModel(true, this)),
    m_requests(),
    m_updating(false),
    m_updateDisabled(false),
    m_dirty(false),
    m_selectAllAction(new QAction(tr("Select All"), this)),
    m_exportAction(new QAction(tr("Export Selection to File..."), this)),
    m_clipboardAction(new QAction(tr("Copy Selection to Clipboard"), this)),
    m_lastMouseButton(Qt::NoButton),
    m_allData(),
    m_availStorages()
{
    ui->setupUi(this);
    setWarningStyle(ui->warningLabel);

    auto refreshButton = new QPushButton(ui->tabWidget);
    refreshButton->setText(tr("Refresh"));
    refreshButton->resize(refreshButton->sizeHint());

    auto anchor = new QnWidgetAnchor(refreshButton);
    anchor->setEdges(Qt::RightEdge | Qt::TopEdge);

    ui->tabWidget->tabBar()->setProperty(style::Properties::kTabShape,
        static_cast<int>(style::TabShape::Compact));

    setupTableView(ui->statsTable, m_model);
    setupTableView(ui->forecastTable, m_forecastModel);

    connect(m_clipboardAction, &QAction::triggered,
        this, &QnStorageAnalyticsWidget::at_clipboardAction_triggered);

    connect(m_exportAction, &QAction::triggered,
        this, &QnStorageAnalyticsWidget::at_exportAction_triggered);

    connect(refreshButton, &QAbstractButton::clicked,
        this, &QnStorageAnalyticsWidget::updateData);

    connect(ui->extraSpaceSlider, &QSlider::valueChanged,
        this, &QnStorageAnalyticsWidget::at_forecastParamsChanged);

    connect(ui->extraSizeSpinBox, QnDoubleSpinBoxValueChanged,
        this, &QnStorageAnalyticsWidget::at_forecastParamsChanged);

    connect(m_selectAllAction, &QAction::triggered,
        this, [this]() { currentTable()->selectAll(); });

    setHelpTopic(this, Qn::ServerSettings_StorageAnalitycs_Help);
}

QnStorageAnalyticsWidget::~QnStorageAnalyticsWidget()
{
}

QnTableView* QnStorageAnalyticsWidget::currentTable() const
{
    return ui->tabWidget->currentWidget() == ui->statsTab
        ? ui->statsTable
        : ui->forecastTable;
}

void QnStorageAnalyticsWidget::setupTableView(QnTableView* table, QAbstractItemModel* model)
{
    auto sortModel = new QnSortedRecordingStatsModel(this);
    sortModel->setSourceModel(model);
    table->setModel(sortModel);
    table->setItemDelegate(new QnRecordingStatsItemDelegate(this));
    table->setItemDelegateForColumn(QnRecordingStatsModel::CameraNameColumn,
        new QnResourceItemDelegate(this));

    CustomHorizontalHeader* headers = new CustomHorizontalHeader(this);
    table->setHorizontalHeader(headers);

    headers->setSectionsClickable(true);
    headers->setSectionResizeMode(QHeaderView::Fixed);
    headers->setSectionResizeMode(QnRecordingStatsModel::CameraNameColumn, QHeaderView::Stretch);
    headers->setMinimumSectionSize(kMinimumSectionSize);
    headers->setSortIndicatorShown(true);

    for (int i = QnRecordingStatsModel::BytesColumn; i < QnRecordingStatsModel::ColumnCount; ++i)
        table->setColumnWidth(i, kMinimumSectionSize);

    m_selectAllAction->setShortcut(QKeySequence::SelectAll);
    m_clipboardAction->setShortcut(QKeySequence::Copy);

    QnSingleEventSignalizer* mouseSignalizer = new QnSingleEventSignalizer(this);
    mouseSignalizer->setEventType(QEvent::MouseButtonRelease);
    table->viewport()->installEventFilter(mouseSignalizer);
    connect(mouseSignalizer, &QnAbstractEventSignalizer::activated, this, &QnStorageAnalyticsWidget::at_mouseButtonRelease);

    table->addAction(m_clipboardAction);
    table->addAction(m_exportAction);

    connect(table, &QTableView::customContextMenuRequested, this, &QnStorageAnalyticsWidget::at_eventsGrid_customContextMenuRequested);

    /* For now bitrates in Statistics and Forecast are linked. */
    //TODO: #vkutin #GDM in the future maybe make independent settings and queries.

    auto otherTable = table == ui->statsTable
        ? ui->forecastTable
        : ui->statsTable;

    connect(headers->comboBox(), QnComboboxCurrentIndexChanged, this, [this, otherTable](int index)
    {
        static_cast<CustomHorizontalHeader*>(otherTable->horizontalHeader())->comboBox()->setCurrentIndex(index);
        if (otherTable == ui->forecastTable)
            updateData();
    });
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
}

void QnStorageAnalyticsWidget::loadDataToUi()
{
    ui->tabWidget->setCurrentWidget(ui->statsTab);
    updateData();
}

void QnStorageAnalyticsWidget::applyChanges()
{
    /* This widget is read-only. */
}

bool QnStorageAnalyticsWidget::hasChanges() const
{
    return false;
}

void QnStorageAnalyticsWidget::updateData()
{
    if (m_updateDisabled)
    {
        m_dirty = true;
        return;
    }

    m_updateDisabled = true;

    qint64 durationForBitrate = static_cast<CustomHorizontalHeader*>(ui->statsTable->horizontalHeader())->durationForBitrate();
    query(durationForBitrate * 1000ll);

    // update UI

    if (!m_requests.isEmpty())
    {
        ui->statsTable->setDisabled(true);
        ui->forecastTable->setDisabled(true);
        ui->stackedWidget->setCurrentWidget(ui->gridPage);
        setCursor(Qt::BusyCursor);
    }
    else
    {
        requestFinished(); // just clear grid
        ui->stackedWidget->setCurrentWidget(ui->warnPage);
    }

    m_updateDisabled = false;
    m_dirty = false;
}

void QnStorageAnalyticsWidget::query(qint64 bitrateAnalizePeriodMs)
{
    m_requests.clear();
    m_allData.clear();
    m_availStorages.clear();

    if (!m_server)
        return;

    if (m_server->getStatus() == Qn::Online)
    {
        int handle = m_server->apiConnection()->getRecordingStatisticsAsync(
            bitrateAnalizePeriodMs,
            this, SLOT(at_gotStatiscits(int, const QnRecordingStatsReply&, int)));

        m_requests.insert(handle, m_server->getId());
        handle = m_server->apiConnection()->getStorageSpaceAsync(false, this, SLOT(at_gotStorageSpace(int, const QnStorageSpaceReply&, int)));
        m_requests.insert(handle, m_server->getId());
    }
}

void QnStorageAnalyticsWidget::at_gotStatiscits(int status, const QnRecordingStatsReply& data, int requestNum)
{
    if (!m_requests.contains(requestNum))
        return;

    QnUuid serverId = m_requests.value(requestNum);
    m_requests.remove(requestNum);

    if (status == 0 && !data.isEmpty())
    {
        for (const auto& value: data)
            m_allData << value;
    }

    if (m_requests.isEmpty())
        requestFinished();
}

void QnStorageAnalyticsWidget::at_gotStorageSpace(int status, const QnStorageSpaceReply& data, int requestNum)
{
    if (!m_requests.contains(requestNum))
        return;

    m_requests.remove(requestNum);
    if (status == 0)
    {
        for (const auto& storage : data.storages)
            m_availStorages << storage;
    }

    if (m_requests.isEmpty())
        requestFinished();
}

void QnStorageAnalyticsWidget::requestFinished()
{
    QnRecordingStatsReply existsCameras;
    QnRecordingStatsReply hiddenCameras;

    for (const auto& camera : m_allData)
    {
        const auto& cam = qnResPool->getResourceByUniqueId(camera.uniqueId);
        if (cam && cam->getParentId() == m_server->getId())
            existsCameras << camera;
        else
            hiddenCameras << camera; // hide all cameras which belong to another server
    }

    if (!hiddenCameras.isEmpty())
    {
        QnCamRecordingStatsData extraRecord; // data occuped by foreign cameras and cameras missed at resource pool
        extraRecord.uniqueId = QnSortedRecordingStatsModel::kForeignCameras;
        for (const auto& hiddenRecord: hiddenCameras)
            extraRecord.recordedBytes += hiddenRecord.recordedBytes;

        existsCameras << extraRecord;
    }

    m_model->setModelData(existsCameras);
    ui->statsTable->setDisabled(false);
    ui->forecastTable->setDisabled(false);
    at_forecastParamsChanged();
    setCursor(Qt::ArrowCursor);
}

void QnStorageAnalyticsWidget::at_eventsGrid_customContextMenuRequested(const QPoint& point)
{
    Q_UNUSED(point);

    auto table = currentTable();

    QScopedPointer<QMenu> menu;
    QModelIndex idx = table->currentIndex();
    if (idx.isValid())
    {
        QnResourcePtr resource = table->model()->data(idx, Qn::ResourceRole).value<QnResourcePtr>();
        QnActionManager *manager = context()->menu();

        if (resource)
        {
            QnActionParameters parameters(resource);
            parameters.setArgument(Qn::NodeTypeRole, Qn::ResourceNode);

            menu.reset(manager->newMenu(Qn::TreeScope, nullptr, parameters));
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

void QnStorageAnalyticsWidget::at_mouseButtonRelease(QObject* sender, QEvent* event)
{
    Q_UNUSED(sender)
    QMouseEvent* me = dynamic_cast<QMouseEvent*> (event);
    m_lastMouseButton = me->button();
}

qint64 QnStorageAnalyticsWidget::sliderPositionToBytes(int value) const
{
    if (value == 0)
        return 0;

    int idx = value / kTicksPerInterval;
    if (idx >= kExtraDataBase.size() - 1)
        return kExtraDataBase.back();

    qint64 k1 = kExtraDataBase[idx];
    qint64 k2 = kExtraDataBase[idx+1];
    int intervalTicks = value % kTicksPerInterval;

    qint64 step = (k2 - k1) / kTicksPerInterval;
    return k1 + step * intervalTicks;
}

int QnStorageAnalyticsWidget::bytesToSliderPosition (qint64 value) const
{
    int idx = 0;
    for (; idx < kExtraDataBase.size() - 1; ++idx)
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

    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

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

QnRecordingStatsReply QnStorageAnalyticsWidget::getForecastData(qint64 extraSizeBytes)
{
    const QnRecordingStatsReply modelData = m_model->modelData();
    ForecastData forecastData;

    // 1. collect camera related forecast params
    bool hasExpaned = false;
    for (const auto& cameraStats: modelData)
    {
        ForecastDataPerCamera cameraForecast;

        QnSecurityCamResourcePtr camRes = qnResPool->getResourceByUniqueId<QnSecurityCamResource>(cameraStats.uniqueId);
        if (camRes)
        {
            cameraForecast.expand = !camRes->isScheduleDisabled();
            cameraForecast.expand &= (camRes->getStatus() == Qn::Online || camRes->getStatus() == Qn::Recording);
            cameraForecast.expand &= cameraStats.archiveDurationSecs > 0 && cameraStats.recordedBytes > 0;
            cameraForecast.minDays = qMax(0, camRes->minDays());
            cameraForecast.maxDays = qMax(0, camRes->maxDays());
            cameraForecast.byterate = cameraStats.recordedBytes / cameraStats.archiveDurationSecs;

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
                for (const auto& storageSpaceData: m_availStorages)
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
    for (const auto& storageSpaceData: m_availStorages)
    {
        QnResourcePtr storageRes = qnResPool->getResourceById(storageSpaceData.storageId);
        if (!storageRes)
            continue;

        if (storageSpaceData.isUsedForWriting && storageSpaceData.isWritable)
            forecastData.totalSpace += qMax(0ll, storageSpaceData.freeSpace - storageSpaceData.reservedSpace);
    }

    // 2.2 add user extra data
    forecastData.totalSpace += extraSizeBytes;

    return doForecast(std::move(forecastData));
}

QnRecordingStatsReply QnStorageAnalyticsWidget::doForecast(ForecastData forecastData)
{
    std::set<qint64> steps; // select possible values for minDays variable
    for (const auto& camera: forecastData.cameras)
    {
        if (camera.minDays > 0)
            steps.insert(camera.minDays * kSecondsPerDay);
    }
    for (qint64 seconds : steps)
    {
        spendData(forecastData, seconds, [seconds](const ForecastDataPerCamera& stats)
        {
            return stats.expand && stats.minDays * kSecondsPerDay >= seconds;
        });
    }

    for (const auto& camera: forecastData.cameras)
    {
        if (camera.maxDays > 0)
            steps.insert(camera.maxDays * kSecondsPerDay);
    }

    steps.insert(kFinalStepSeconds); // final step for all cameras

    for (qint64 seconds: steps)
    {
        spendData(forecastData, seconds, [seconds](const ForecastDataPerCamera& stats)
        {
            return stats.expand && (stats.maxDays * kSecondsPerDay >= seconds || stats.maxDays == 0);
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

void QnStorageAnalyticsWidget::spendData(ForecastData& forecastData, qint64 needSeconds, std::function<bool (const ForecastDataPerCamera& stats)> predicate)
{
    qint64 moreBytesRequired = 0;
    for (ForecastDataPerCamera& cameraForecast: forecastData.cameras)
    {
        if (!predicate(cameraForecast))
            continue;

        qint64 needMoreSeconds = qMax(0ll, needSeconds - cameraForecast.stats.archiveDurationSecs);
        moreBytesRequired += needMoreSeconds * cameraForecast.byterate;
    }

    // we have less bytes left then required
    qreal coeff = 1.0;
    if (moreBytesRequired > forecastData.totalSpace)
        coeff = forecastData.totalSpace / (qreal) moreBytesRequired;

    // spend data
    for (ForecastDataPerCamera& cameraForecast: forecastData.cameras)
    {
        if (predicate(cameraForecast))
            cameraForecast.stats.archiveDurationSecs += coeff * (needSeconds - cameraForecast.stats.archiveDurationSecs);
    }

    forecastData.totalSpace = qMax(0ll, forecastData.totalSpace - moreBytesRequired);
}
