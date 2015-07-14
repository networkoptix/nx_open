#include "recording_stats_dialog.h"
#include "ui_recording_stats_dialog.h"

#include <QtCore/QMimeData>
#include <QtGui/QClipboard>
#include <QtWidgets/QMenu>
#include <QtGui/QMouseEvent>

#include <core/resource_management/resource_pool.h>
#include <client/client_globals.h>
#include <client/client_settings.h>
#include <plugins/resource/server_camera/server_camera.h>
#include <ui/common/grid_widget_helper.h>
#include <ui/help/help_topic_accessor.h>
#include <ui/help/help_topics.h>
#include <ui/dialogs/custom_file_dialog.h>
#include <ui/style/resource_icon_cache.h>
#include <ui/style/skin.h>
#include <ui/style/warning_style.h>

#include <ui/workbench/workbench_context.h>
#include <ui/workaround/widgets_signals_workaround.h>
#include "ui/models/recording_stats_model.h"
#include <ui/actions/action_manager.h>
#include <ui/actions/actions.h>
#include <core/resource/media_server_resource.h>
#include "utils/common/event_processors.h"
#include <utils/common/scoped_painter_rollback.h>
#include "utils/math/color_transformations.h"
#include "utils/common/synctime.h"

namespace {
    const qint64 MSECS_PER_DAY = 1000ll * 3600ll * 24ll;
    const qint64 SECS_PER_DAY = 3600 * 24;
    const qint64 SECS_PER_WEEK = SECS_PER_DAY * 7;
    const qreal FORECAST_STEP = 3600ll; // 1 hour
}

void QnRecordingStatsItemDelegate::paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const
{
    Q_ASSERT(index.isValid());

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

        qreal realData = index.data(Qn::RecordingStatChartDataRole).toReal();
        qreal forecastData = index.data(Qn::RecordingStatForecastDataRole).toReal();

        QColor baseColor = opt.backgroundBrush.color(); //opt.palette.color(QPalette::Normal, QPalette::Base);
        QColor realColor(7, 98, 129);
        QColor forecastColor(12, 81, 105);
        
        if (opt.state & QStyle::State_Selected) {
            // alternate row color
            const int shift = 16;
            baseColor = shiftColor(baseColor, shift, shift, shift);
            realColor = shiftColor(realColor, shift, shift, shift);
            forecastColor = shiftColor(forecastColor, shift, shift, shift);
        }
        painter->fillRect(opt.rect, baseColor);

        opt.rect.setWidth(opt.rect.width() - 4);
        
        painter->fillRect(QRect(0,0, opt.rect.width() * forecastData, opt.rect.height()), forecastColor);
        painter->fillRect(QRect(0,0, opt.rect.width() * realData, opt.rect.height()), realColor);

        painter->setFont(opt.font);
        //QString text = index.data().toString();
        //QFontMetrics fm(opt.font);
        //QSize textSize = fm.size(Qt::TextSingleLine, text);
        //painter->drawText(opt.rect.width() - textSize.width() - 4, (opt.rect.height()+fm.ascent())/2, text);
        painter->drawText(opt.rect, Qt::AlignRight | Qt::AlignVCenter, index.data().toString());

    }
    else {
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);
    }
}


QnRecordingStatsDialog::QnRecordingStatsDialog(QWidget *parent):
    base_type(parent),
    ui(new Ui::RecordingStatsDialog),
    m_updateDisabled(false),
    m_dirty(false),
    m_lastMouseButton(Qt::NoButton)
{
    ui->setupUi(this);
    setWarningStyle(ui->warningLabel);

    //setHelpTopic(this, Qn::MainWindow_Notifications_EventLog_Help);
    m_model = new QnRecordingStatsModel(this);

    QnSortedRecordingStatsModel* sortModel = new QnSortedRecordingStatsModel(this);
    sortModel->setSourceModel(m_model);
    ui->gridEvents->setModel(sortModel);
    ui->gridEvents->setItemDelegate(new QnRecordingStatsItemDelegate(this));

    QHeaderView* headers = ui->gridEvents->horizontalHeader();
    headers->setSectionResizeMode(QHeaderView::Interactive);
    headers->setSectionResizeMode(QnRecordingStatsModel::CameraNameColumn, QHeaderView::ResizeToContents);

    m_clipboardAction   = new QAction(tr("Copy Selection to Clipboard"), this);
    m_exportAction      = new QAction(tr("Export Selection to File..."), this);
    m_selectAllAction   = new QAction(tr("Select All"), this);
    m_selectAllAction->setShortcut(QKeySequence::SelectAll);
    m_clipboardAction->setShortcut(QKeySequence::Copy);

    QnSingleEventSignalizer *mouseSignalizer = new QnSingleEventSignalizer(this);
    mouseSignalizer->setEventType(QEvent::MouseButtonRelease);
    ui->gridEvents->viewport()->installEventFilter(mouseSignalizer);
    connect(mouseSignalizer, &QnAbstractEventSignalizer::activated, this, &QnRecordingStatsDialog::at_mouseButtonRelease);

    ui->gridEvents->addAction(m_clipboardAction);
    ui->gridEvents->addAction(m_exportAction);

    ui->loadingProgressBar->hide();

    connect(m_clipboardAction,      &QAction::triggered,                this,   &QnRecordingStatsDialog::at_clipboardAction_triggered);
    connect(m_exportAction,         &QAction::triggered,                this,   &QnRecordingStatsDialog::at_exportAction_triggered);
    connect(m_selectAllAction,      &QAction::triggered,                ui->gridEvents, &QTableView::selectAll);

    connect(ui->refreshButton,      &QAbstractButton::clicked,          this,   &QnRecordingStatsDialog::updateData);

    connect(ui->gridEvents,         &QTableView::clicked,               this,   &QnRecordingStatsDialog::at_eventsGrid_clicked);
    connect(ui->gridEvents,         &QTableView::customContextMenuRequested, this, &QnRecordingStatsDialog::at_eventsGrid_customContextMenuRequested);
    connect(qnSettings->notifier(QnClientSettings::IP_SHOWN_IN_TREE), &QnPropertyNotifier::valueChanged, ui->gridEvents, &QAbstractItemView::reset);

    connect(ui->checkBoxForecast,   &QCheckBox::stateChanged, this, &QnRecordingStatsDialog::at_forecastParamsChanged);
    connect(ui->extraSpaceSlider,   &QSlider::valueChanged,   this, &QnRecordingStatsDialog::at_forecastParamsChanged);
    

    ui->mainGridLayout->activate();
}

QnRecordingStatsDialog::~QnRecordingStatsDialog() {
}

void QnRecordingStatsDialog::updateData()
{
    if (m_updateDisabled) {
        m_dirty = true;
        return;
    }
    m_updateDisabled = true;

    static const int bitrateAnalizePeriodMs = 1000 * 60 * 5; // todo: fill me!
    query(bitrateAnalizePeriodMs);

    // update UI

    if (!m_requests.isEmpty()) {
        ui->gridEvents->setDisabled(true);
        ui->stackedWidget->setCurrentWidget(ui->gridPage);
        setCursor(Qt::BusyCursor);
        ui->loadingProgressBar->show();
    }
    else {
        requestFinished(); // just clear grid
        ui->stackedWidget->setCurrentWidget(ui->warnPage);
    }

    m_updateDisabled = false;
    m_dirty = false;
}

QList<QnMediaServerResourcePtr> QnRecordingStatsDialog::getServerList() const
{
    QList<QnMediaServerResourcePtr> result;
    QnResourceList resList = qnResPool->getAllResourceByTypeName(lit("Server"));
    foreach(const QnResourcePtr& r, resList) {
        QnMediaServerResourcePtr mServer = r.dynamicCast<QnMediaServerResource>();
        if (mServer)
            result << mServer;
    }

    return result;
}

void QnRecordingStatsDialog::query(qint64 bitrateAnalizePeriodMs)
{
    m_requests.clear();
    m_allData.clear();
    m_availStorages.clear();

    QList<QnMediaServerResourcePtr> mediaServerList = getServerList();
    foreach(const QnMediaServerResourcePtr& mserver, mediaServerList)
    {
        if (mserver->getStatus() == Qn::Online)
        {
            int handle = mserver->apiConnection()->getRecordingStatisticsAsync(
                bitrateAnalizePeriodMs,
                this, SLOT(at_gotStatiscits(int, const QnRecordingStatsReply&, int)));
            m_requests.insert(handle, mserver->getId());
            handle = mserver->apiConnection()->getStorageSpaceAsync(
                this, SLOT(at_gotStorageSpace(int, const QnStorageSpaceReply&, int)));
            m_requests.insert(handle, mserver->getId());
        }
    }
}

void QnRecordingStatsDialog::at_gotStatiscits(int httpStatus, const QnRecordingStatsReply& data, int requestNum)
{
    if (!m_requests.contains(requestNum))
        return;
    QnUuid serverId = m_requests.value(requestNum);
    m_requests.remove(requestNum);
    if (httpStatus == 0 && !data.isEmpty()) {
        for (const auto& value: data)
            m_allData << value;
    }
    if (m_requests.isEmpty()) {
        requestFinished();
    }
}

void QnRecordingStatsDialog::at_gotStorageSpace(int httpStatus, const QnStorageSpaceReply& data, int requestNum)
{
    if (!m_requests.contains(requestNum))
        return;
    m_requests.remove(requestNum);
    if (httpStatus == 0) {
        for (const auto& storage: data.storages) 
            m_availStorages << storage;
    }
    if (m_requests.isEmpty()) {
        requestFinished();
    }
}

void QnRecordingStatsDialog::requestFinished()
{
    QnRecordingStatsReply existsCameras;
    m_hidenCameras.clear();
    for (const auto& camera: m_allData) 
    {
        if (qnResPool->hasSuchResource(camera.uniqueId))
            existsCameras << camera;
        else
            m_hidenCameras << camera;
    }
    m_model->setModelData(existsCameras);

    
    ui->gridEvents->setDisabled(false);
    setCursor(Qt::ArrowCursor);
    ui->loadingProgressBar->hide();
}

void QnRecordingStatsDialog::at_eventsGrid_clicked(const QModelIndex& idx)
{
}

void QnRecordingStatsDialog::at_eventsGrid_customContextMenuRequested(const QPoint&)
{
    QMenu* menu = 0;
    QModelIndex idx = ui->gridEvents->currentIndex();
    if (idx.isValid())
    {
        QnResourcePtr resource = ui->gridEvents->model()->data(idx, Qn::ResourceRole).value<QnResourcePtr>();
        QnActionManager *manager = context()->menu();
        if (resource) {
            menu = manager->newMenu(Qn::TreeScope, this, QnActionParameters(resource));
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

void QnRecordingStatsDialog::at_exportAction_triggered()
{
    QnGridWidgetHelper::exportToFile(ui->gridEvents, this, tr("Export selected events to file"));
}

void QnRecordingStatsDialog::at_clipboardAction_triggered()
{
    QnGridWidgetHelper::copyToClipboard(ui->gridEvents);
}

void QnRecordingStatsDialog::at_mouseButtonRelease(QObject* sender, QEvent* event)
{
    Q_UNUSED(sender)
    QMouseEvent* me = dynamic_cast<QMouseEvent*> (event);
    m_lastMouseButton = me->button();
}

void QnRecordingStatsDialog::disableUpdateData()
{
    m_updateDisabled = true;
}

void QnRecordingStatsDialog::enableUpdateData()
{
    m_updateDisabled = false;
    if (m_dirty) {
        m_dirty = false;
        if (isVisible())
            updateData();
    }
}

void QnRecordingStatsDialog::setVisible(bool value)
{
    // TODO: #Elric use showEvent instead. 

    if (value && !isVisible())
        updateData();
    QDialog::setVisible(value);
}

void QnRecordingStatsDialog::at_forecastParamsChanged()
{
    ui->gridEvents->setEnabled(false);
    static const qint64 gb = 1000000000ll;
    static const qint64 tb = 1000ll * gb;
    static const qint64 EXTRA_DATA[] = {0, 100 * gb, tb, 10 * tb, 100 * tb};
    if (ui->checkBoxForecast->isChecked()) {
        int idx = ui->extraSpaceSlider->value();
        m_model->setForecastData(getForecastData(EXTRA_DATA[idx]));
    }
    else
        m_model->setForecastData(QnRecordingStatsReply());
    ui->gridEvents->setEnabled(true);
}

QnRecordingStatsReply QnRecordingStatsDialog::getForecastData(qint64 extraSizeBytes)
{
    const QnRecordingStatsReply modelData = m_model->modelData();
    ForecastData forecastData;
    qint64 currentTimeMs = qnSyncTime->currentMSecsSinceEpoch();

    // 1. collect camera related forecast params
    for(const auto& cameraStats: modelData) 
    {
        if (cameraStats.uniqueId.isNull())
            continue;
        QnSecurityCamResourcePtr camRes = qnResPool->getResourceByUniqueId<QnSecurityCamResource>(cameraStats.uniqueId);
        if (!camRes)
            continue;

        ForecastDataPerCamera cameraForecast;
        //cameraForecast.archiveDurationSecs =  (currentTimeMs - cameraStats.archiveStartTimeMs) / 1000;
        cameraForecast.stats = cameraStats;
        cameraForecast.expand = !camRes->isScheduleDisabled();
        cameraForecast.expand &= (camRes->getStatus() == Qn::Online || camRes->getStatus() == Qn::Recording);
        cameraForecast.expand &= cameraForecast.stats.archiveDurationSecs > 0 && cameraForecast.stats.recordedBytes > 0;
        cameraForecast.maxDays = camRes->maxDays();
        qint64 calendarBitrate = cameraForecast.stats.recordedBytes / cameraForecast.stats.archiveDurationSecs;
        cameraForecast.bytesPerStep = calendarBitrate * FORECAST_STEP;
        cameraForecast.usageCoeff = cameraForecast.stats.recordedSecs / (qreal) cameraForecast.stats.archiveDurationSecs;
        Q_ASSERT(qBetween(0.0, cameraForecast.usageCoeff, 1.00001));
        forecastData.cameras.push_back(std::move(cameraForecast));
    }

    // 2.1 add extra space
    for (const auto& storageSpaceData: m_availStorages) 
    {
        QnResourcePtr storageRes = qnResPool->getResourceById(storageSpaceData.storageId);
        if (!storageRes)
            continue;
        if (storageSpaceData.isUsedForWriting && storageSpaceData.isWritable) 
            forecastData.extraSpace += qMax(0ll, storageSpaceData.freeSpace - storageSpaceData.reservedSpace);
    }
    // 2.2 take into account archive of removed cameras
    for (const auto& hiddenCam: m_hidenCameras)
        forecastData.extraSpace += hiddenCam.recordedBytes;

    // 2.3 add user extra data
    forecastData.extraSpace += extraSizeBytes;

    return doForecastMainStep(forecastData);;
}

QnRecordingStatsReply QnRecordingStatsDialog::doForecastMainStep(ForecastData& forecastData)
{
    // Go to the future, one step is a one day and spend extraSpace
    qint64 prevExtraSpace = -1;
    while (forecastData.extraSpace > 0 && forecastData.extraSpace != prevExtraSpace)
    {
        prevExtraSpace = forecastData.extraSpace;
        for (ForecastDataPerCamera& cameraForecast: forecastData.cameras)
        {
            if (!cameraForecast.expand)
                continue;
            if (cameraForecast.maxDays > 0 && cameraForecast.stats.archiveDurationSecs * SECS_PER_DAY >= cameraForecast.maxDays)
                continue; // this camera reached the limit of max recorded archive days
            cameraForecast.stats.archiveDurationSecs += FORECAST_STEP;
            cameraForecast.stats.recordedBytes += cameraForecast.bytesPerStep;
            cameraForecast.stats.recordedSecs += FORECAST_STEP * cameraForecast.usageCoeff;
            
            forecastData.extraSpace -= cameraForecast.bytesPerStep;
            if (forecastData.extraSpace <= 0)
                break;
        }
    }

    QnRecordingStatsReply result;
    for (const auto& value: forecastData.cameras)
        result << value.stats;
    return result;
}
