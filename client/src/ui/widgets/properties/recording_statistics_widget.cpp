#include "recording_statistics_widget.h"
#include "ui_recording_statistics_widget.h"

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

#include <ui/common/grid_widget_helper.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/dialogs/custom_file_dialog.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/style/skin.h>
#include <ui/style/custom_style.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include "ui/models/recording_stats_model.h"
#include <ui/actions/action_manager.h>
#include <ui/actions/actions.h>

#include "utils/common/event_processors.h"
#include <utils/common/scoped_painter_rollback.h>
#include <utils/common/scoped_value_rollback.h>
#include "utils/math/color_transformations.h"
#include "utils/common/synctime.h"
#include <set>

namespace {

    const qint64 SECS_PER_HOUR = 3600;
    const qint64 SECS_PER_DAY = SECS_PER_HOUR * 24;
    const qint64 BYTES_IN_GB = 1000000000ll;
    const qint64 BYTES_IN_TB = 1000ll * BYTES_IN_GB;
    const qint64 FINAL_STEP_SECONDS = 1000000000ll * 10;

    //TODO: #rvasilenko refactor all algorithms working with EXTRA_DATA_BASE to STL
    const std::array<qint64, 5> EXTRA_DATA_BASE = {
        10 * BYTES_IN_GB,
        1 * BYTES_IN_TB,
        10 * BYTES_IN_TB,
        100 * BYTES_IN_TB,
        1000 * BYTES_IN_TB
    };
    const int TICKS_PER_INTERVAL = 100;
    const int SPACE_INTERVAL = 4;

    class QnRecordingStatsItemDelegate: public QStyledItemDelegate
    {
        typedef QStyledItemDelegate base_type;

    public:
        explicit QnRecordingStatsItemDelegate(QObject *parent = NULL):
            base_type(parent)
        {}

        virtual void paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const override {
            NX_ASSERT(index.isValid());

            QStyleOptionViewItem opt = option;
            initStyleOption(&opt, index);

            const QWidget *widget = option.widget;
            QStyle *style = widget ? widget->style() : QApplication::style();

            if (index.column() != QnRecordingStatsModel::CameraNameColumn)
            {
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


                QColor baseColor = opt.backgroundBrush.color(); //opt.palette.color(QPalette::Normal, QPalette::Base);

                if (opt.state & QStyle::State_Selected) {
                    // alternate row color
                    const int shift = 16;
                    baseColor = shiftColor(baseColor, shift, shift, shift);
                }
                painter->fillRect(opt.rect, baseColor);

                opt.rect.adjust(2, 1, -2, -1);

                painter->fillRect(QRect(opt.rect.left() , opt.rect.top(), opt.rect.width() * chartData, opt.rect.height()), chartColor);

                painter->setFont(opt.font);
                painter->setPen(opt.palette.foreground().color());
                painter->drawText(opt.rect, Qt::AlignRight | Qt::AlignVCenter, index.data().toString());

            }
            else {
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
            QRect rect(sectionViewportPosition(QnRecordingStatsModel::BitrateColumn), 0,  sectionSize(QnRecordingStatsModel::BitrateColumn), height());
            int width = m_comboBox->minimumSizeHint().width();
            rect.adjust(qMax(SPACE_INTERVAL, rect.width() - width), 0, -SPACE_INTERVAL, 0);
            m_comboBox->setGeometry(rect);
        }

    public:
        CustomHorizontalHeader(QWidget *parent = 0) : QHeaderView(Qt::Horizontal, parent)
        {
            m_comboBox = new QComboBox(this);
            m_comboBox->addItem(tr("5 minutes"), 5 * 60);
            m_comboBox->addItem(tr("Hour"),  60 * 60);
            m_comboBox->addItem(tr("Day"),   3600 * 24);
            m_comboBox->addItem(tr("Week"),  3600 * 24 * 7);
            m_comboBox->addItem(tr("Month"), 3600 * 24 * 30);
            m_comboBox->addItem(tr("All Data"), 0);
            m_comboBox->setCurrentIndex(m_comboBox->count()-1);

            connect(this, &QHeaderView::sectionResized, this, [this]() { updateComboBox();}, Qt::DirectConnection);
            connect(this, &QHeaderView::sectionResized, this, [this]() { updateComboBox();}, Qt::QueuedConnection);
        }

        virtual void showEvent(QShowEvent *e) override
        {
            QHeaderView::showEvent(e);
            updateComboBox();
            m_comboBox->show();
        }

        virtual void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override
        {
            QHeaderView::paintSection(painter, rect, logicalIndex);
            if (logicalIndex == QnRecordingStatsModel::BitrateColumn)
            {
                QnScopedPainterFontRollback rollback(painter);
                QnScopedPainterPenRollback rollback2(painter);
                painter->setFont(font());
                painter->setPen(palette().foreground().color());
                int width = m_comboBox->minimumSizeHint().width();
                QRect r(rect);
                r.adjust(0, 0, -(width + SPACE_INTERVAL), 0);
                painter->drawText(r, Qt::AlignRight | Qt::AlignVCenter, tr("Bitrate for the Last Recorded"));
            }
        }

        QComboBox* comboBox() const {
            return m_comboBox;
        }

        int durationForBitrate() const {
            return m_comboBox->itemData(m_comboBox->currentIndex()).toInt();
        }
    };

    const int minimumSectionSize = 200;
}

QnRecordingStatisticsWidget::QnRecordingStatisticsWidget(QWidget* parent /* = 0*/):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    ui(new Ui::RecordingStatisticsWidget),
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

    auto sortModel = new QnSortedRecordingStatsModel(this);
    sortModel->setSourceModel(m_model);
    ui->gridEvents->setModel(sortModel);
    ui->gridEvents->setItemDelegate(new QnRecordingStatsItemDelegate(this));

    CustomHorizontalHeader* headers = new CustomHorizontalHeader(this);
    ui->gridEvents->setHorizontalHeader(headers);

    headers->setSectionsClickable(true);
    headers->setStretchLastSection(true);
    headers->setSectionResizeMode(QHeaderView::Interactive);
    headers->setSectionResizeMode(QnRecordingStatsModel::CameraNameColumn, QHeaderView::ResizeToContents);
    headers->setMinimumSectionSize(minimumSectionSize);
    headers->setSortIndicatorShown(true);

    for (int i = QnRecordingStatsModel::BytesColumn; i < QnRecordingStatsModel::ColumnCount; ++i)
        ui->gridEvents->setColumnWidth(i, minimumSectionSize);

    m_selectAllAction->setShortcut(QKeySequence::SelectAll);
    m_clipboardAction->setShortcut(QKeySequence::Copy);

    QnSingleEventSignalizer *mouseSignalizer = new QnSingleEventSignalizer(this);
    mouseSignalizer->setEventType(QEvent::MouseButtonRelease);
    ui->gridEvents->viewport()->installEventFilter(mouseSignalizer);
    connect(mouseSignalizer, &QnAbstractEventSignalizer::activated, this, &QnRecordingStatisticsWidget::at_mouseButtonRelease);

    ui->gridEvents->addAction(m_clipboardAction);
    ui->gridEvents->addAction(m_exportAction);

    connect(m_clipboardAction,      &QAction::triggered,                    this,   &QnRecordingStatisticsWidget::at_clipboardAction_triggered);
    connect(m_exportAction,         &QAction::triggered,                    this,   &QnRecordingStatisticsWidget::at_exportAction_triggered);
    connect(headers->comboBox(),    QnComboboxCurrentIndexChanged,          this,   &QnRecordingStatisticsWidget::updateData);
    connect(ui->refreshButton,      &QAbstractButton::clicked,              this,   &QnRecordingStatisticsWidget::updateData);
    connect(ui->checkBoxForecast,   &QCheckBox::stateChanged,               this,   &QnRecordingStatisticsWidget::at_forecastParamsChanged);
    connect(ui->extraSpaceSlider,   &QSlider::valueChanged,                 this,   &QnRecordingStatisticsWidget::at_forecastParamsChanged);
    connect(ui->extraSizeSpinBox,   QnDoubleSpinBoxValueChanged,            this,   &QnRecordingStatisticsWidget::at_forecastParamsChanged);
    connect(m_model,                &QnRecordingStatsModel::colorsChanged,  this,   &QnRecordingStatisticsWidget::updateColors);
    connect(ui->gridEvents,         &QTableView::customContextMenuRequested,this,   &QnRecordingStatisticsWidget::at_eventsGrid_customContextMenuRequested);
    connect(qnSettings->notifier(QnClientSettings::IP_SHOWN_IN_TREE),
                                    &QnPropertyNotifier::valueChanged,      ui->gridEvents, &QAbstractItemView::reset);
    connect(m_selectAllAction,      &QAction::triggered,                    ui->gridEvents, &QTableView::selectAll);

    updateColors();

    setHelpTopic(this, Qn::ServerSettings_StorageAnalitycs_Help);
}

QnRecordingStatisticsWidget::~QnRecordingStatisticsWidget()
{}

QnMediaServerResourcePtr QnRecordingStatisticsWidget::server() const {
    return m_server;
}

void QnRecordingStatisticsWidget::setServer(const QnMediaServerResourcePtr &server) {
    if (m_server == server)
        return;
    m_server = server;
}

void QnRecordingStatisticsWidget::loadDataToUi() {
    updateData();
}

void QnRecordingStatisticsWidget::applyChanges() {
    /* This widget is read-only. */
}

bool QnRecordingStatisticsWidget::hasChanges() const {
    return false;
}

void QnRecordingStatisticsWidget::updateData() {
    if (m_updateDisabled) {
        m_dirty = true;
        return;
    }
    m_updateDisabled = true;

    qint64 durationForBitrate = static_cast<CustomHorizontalHeader*>(ui->gridEvents->horizontalHeader())->durationForBitrate();
    query(durationForBitrate * 1000ll);

    // update UI

    if (!m_requests.isEmpty()) {
        ui->gridEvents->setDisabled(true);
        ui->stackedWidget->setCurrentWidget(ui->gridPage);
        setCursor(Qt::BusyCursor);
    }
    else {
        requestFinished(); // just clear grid
        ui->stackedWidget->setCurrentWidget(ui->warnPage);
    }

    m_updateDisabled = false;
    m_dirty = false;
}


void QnRecordingStatisticsWidget::updateColors() {
    auto colors = m_model->colors();

    QPalette palette = ui->labelUsageColor->palette();
    palette.setColor(ui->labelUsageColor->backgroundRole(), colors.chartMainColor);
    ui->labelUsageColor->setPalette(palette);
    ui->labelUsageColor->setAutoFillBackground(true);
    ui->labelUsageColor->update();

    palette = ui->labelUsageColor->palette();
    palette.setColor(ui->labelForecastColor->backgroundRole(), colors.chartForecastColor);
    ui->labelForecastColor->setPalette(palette);
    ui->labelForecastColor->setAutoFillBackground(true);
    ui->labelForecastColor->update();
}

void QnRecordingStatisticsWidget::resetForecast() {
    ui->checkBoxForecast->setChecked(false);
}


void QnRecordingStatisticsWidget::query(qint64 bitrateAnalizePeriodMs)
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

void QnRecordingStatisticsWidget::at_gotStatiscits(int status, const QnRecordingStatsReply& data, int requestNum)
{
    if (!m_requests.contains(requestNum))
        return;
    QnUuid serverId = m_requests.value(requestNum);
    m_requests.remove(requestNum);
    if (status == 0 && !data.isEmpty()) {
        for (const auto& value: data)
            m_allData << value;
    }
    if (m_requests.isEmpty()) {
        requestFinished();
    }
}

void QnRecordingStatisticsWidget::at_gotStorageSpace(int status, const QnStorageSpaceReply& data, int requestNum)
{
    if (!m_requests.contains(requestNum))
        return;
    m_requests.remove(requestNum);
    if (status == 0) {
        for (const auto& storage: data.storages) {
            m_availStorages << storage;
        }
    }
    if (m_requests.isEmpty()) {
        requestFinished();
    }
}

void QnRecordingStatisticsWidget::requestFinished()
{
    QnRecordingStatsReply existsCameras;
    QnRecordingStatsReply hiddenCameras;
    for (const auto& camera: m_allData)
    {
        const auto& cam = qnResPool->getResourceByUniqueId(camera.uniqueId);
        if (cam && cam->getParentId() == m_server->getId())
            existsCameras << camera;
        else
            hiddenCameras << camera; // hide all cameras which belong to another server
    }
    if (!hiddenCameras.isEmpty()) {
        QnCamRecordingStatsData extraRecord; // data occuped by foreign cameras and cameras missed at resource pool
        extraRecord.uniqueId = QnSortedRecordingStatsModel::kForeignCameras;
        for (const auto& hiddenRecord: hiddenCameras)
            extraRecord.recordedBytes += hiddenRecord.recordedBytes;
        existsCameras << extraRecord;
    }

    m_model->setModelData(existsCameras);
    ui->gridEvents->setDisabled(false);
    at_forecastParamsChanged();
    setCursor(Qt::ArrowCursor);
}

void QnRecordingStatisticsWidget::at_eventsGrid_customContextMenuRequested(const QPoint&)
{
    QMenu* menu = 0;
    QModelIndex idx = ui->gridEvents->currentIndex();
    if (idx.isValid())
    {
        QnResourcePtr resource = ui->gridEvents->model()->data(idx, Qn::ResourceRole).value<QnResourcePtr>();
        QnActionManager *manager = context()->menu();

        if (resource) {
            QnActionParameters parameters(resource);
            parameters.setArgument(Qn::NodeTypeRole, Qn::ResourceNode);

            menu = manager->newMenu(Qn::TreeScope, this, parameters);
            foreach(QAction* action, menu->actions())
                action->setShortcut(QKeySequence());
        }
    }
    if (menu)
        menu->addSeparator();
    else
        menu = new QMenu(this);

    m_clipboardAction->setEnabled(ui->gridEvents->selectionModel()->hasSelection());
    m_exportAction->setEnabled(ui->gridEvents->selectionModel()->hasSelection());

    menu->addSeparator();

    menu->addAction(m_selectAllAction);
    menu->addAction(m_exportAction);
    menu->addAction(m_clipboardAction);

    menu->exec(QCursor::pos());
    menu->deleteLater();
}

void QnRecordingStatisticsWidget::at_exportAction_triggered()
{
    QnGridWidgetHelper::exportToFile(ui->gridEvents, this, tr("Export selected events to file"));
}

void QnRecordingStatisticsWidget::at_clipboardAction_triggered()
{
    QnGridWidgetHelper::copyToClipboard(ui->gridEvents);
}

void QnRecordingStatisticsWidget::at_mouseButtonRelease(QObject* sender, QEvent* event)
{
    Q_UNUSED(sender)
    QMouseEvent* me = dynamic_cast<QMouseEvent*> (event);
    m_lastMouseButton = me->button();
}

qint64 QnRecordingStatisticsWidget::sliderPositionToBytes(int value) const
{
    if (value == 0)
        return 0;

    int idx = value / TICKS_PER_INTERVAL;
    if (idx >= EXTRA_DATA_BASE.size() - 1)
        return EXTRA_DATA_BASE.back();

    qint64 k1 = EXTRA_DATA_BASE[idx];
    qint64 k2 = EXTRA_DATA_BASE[idx+1];
    int intervalTicks = value % TICKS_PER_INTERVAL;

    qint64 step = (k2 - k1) / TICKS_PER_INTERVAL;
    return k1 + step * intervalTicks;
}

int QnRecordingStatisticsWidget::bytesToSliderPosition (qint64 value) const
{
    int idx = 0;
    for (; idx < EXTRA_DATA_BASE.size() - 1; ++idx)
    {
        if (EXTRA_DATA_BASE[idx+1] >= value)
            break;
    }
    qint64 step = (EXTRA_DATA_BASE[idx+1] - EXTRA_DATA_BASE[idx]) / TICKS_PER_INTERVAL;
    value -= EXTRA_DATA_BASE[idx];
    return idx * TICKS_PER_INTERVAL + value / step;
}

void QnRecordingStatisticsWidget::at_forecastParamsChanged()
{
    if (m_updating)
        return;
    QN_SCOPED_VALUE_ROLLBACK(&m_updating, true);

    QnSortedRecordingStatsModel* sortModel = static_cast<QnSortedRecordingStatsModel*> (ui->gridEvents->model());
    if (ui->checkBoxForecast->isChecked())
        sortModel->setSourceModel(m_forecastModel);
    else
        sortModel->setSourceModel(m_model);

    if (!ui->gridEvents->isEnabled())
        return;
    ui->gridEvents->setEnabled(false);

    bool fcEnabled = ui->checkBoxForecast->isChecked();
    ui->extraSpaceSlider->setEnabled(fcEnabled);
    ui->forecastControlWidget->setEnabled(fcEnabled);

    if (ui->checkBoxForecast->isChecked())
    {
        qint64 forecastedSize = 0;
        if (sender() == ui->extraSpaceSlider) {
            forecastedSize = sliderPositionToBytes(ui->extraSpaceSlider->value());
            ui->extraSizeSpinBox->setValue(forecastedSize / (qreal) BYTES_IN_TB);
        }
        else {
            forecastedSize = ui->extraSizeSpinBox->value() * BYTES_IN_TB;
            ui->extraSpaceSlider->setValue(bytesToSliderPosition(forecastedSize));
        }

        m_forecastModel->setModelData(getForecastData(forecastedSize));
    }
    else
        m_forecastModel->setModelData(QnRecordingStatsReply());
    ui->gridEvents->setEnabled(true);
}

QnRecordingStatsReply QnRecordingStatisticsWidget::getForecastData(qint64 extraSizeBytes)
{
    const QnRecordingStatsReply modelData = m_model->modelData();
    ForecastData forecastData;

    // 1. collect camera related forecast params
    bool hasExpaned = false;
    for(const auto& cameraStats: modelData)
    {
        ForecastDataPerCamera cameraForecast;

        QnSecurityCamResourcePtr camRes = qnResPool->getResourceByUniqueId<QnSecurityCamResource>(cameraStats.uniqueId);
        if (camRes) {
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

QnRecordingStatsReply QnRecordingStatisticsWidget::doForecast(ForecastData forecastData)
{
    std::set<qint64> steps; // select possible values for minDays variable
    for (const auto& camera: forecastData.cameras) {
        if (camera.minDays > 0)
            steps.insert(camera.minDays * SECS_PER_DAY);
    }
    for (qint64 seconds: steps) {
        spendData(forecastData, seconds, [seconds](const ForecastDataPerCamera& stats)
        {
            return stats.expand && stats.minDays * SECS_PER_DAY >= seconds;
        });
    }

    for (const auto& camera: forecastData.cameras) {
        if (camera.maxDays > 0)
            steps.insert(camera.maxDays * SECS_PER_DAY);
    }
    steps.insert(FINAL_STEP_SECONDS); // final step for all cameras

    for (qint64 seconds: steps) {
        spendData(forecastData, seconds, [seconds](const ForecastDataPerCamera& stats)
        {
            return stats.expand && (stats.maxDays * SECS_PER_DAY >= seconds || stats.maxDays == 0);
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

void QnRecordingStatisticsWidget::spendData(ForecastData& forecastData, qint64 needSeconds, std::function<bool (const ForecastDataPerCamera& stats)> predicate)
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
        if (!predicate(cameraForecast))
            continue;
        cameraForecast.stats.archiveDurationSecs += coeff * (needSeconds - cameraForecast.stats.archiveDurationSecs);
    }
    forecastData.totalSpace = qMax(0ll, forecastData.totalSpace - moreBytesRequired);

}
