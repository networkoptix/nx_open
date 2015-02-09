#include "workbench_navigator.h"

#include <cassert>

#include <QtCore/QTimer>

#include <QtWidgets/QAction>
#include <QtWidgets/QMenu>
#include <QtWidgets/QGraphicsSceneContextMenuEvent>
#include <QtWidgets/QApplication>

extern "C"
{
    #include <libavutil/avutil.h> // TODO: remove
}

#include <utils/common/util.h>
#include <utils/common/synctime.h>
#include <utils/common/scoped_value_rollback.h>
#include <utils/common/checked_cast.h>

#include <client/client_settings.h>

#include <camera/resource_display.h>
#include <core/resource/camera_bookmark.h>
#include <core/resource/camera_resource.h>
#include <core/resource/layout_resource.h>
#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>

#include <camera/camera_data_manager.h>
#include <camera/loaders/caching_camera_data_loader.h>
#include <camera/cam_display.h>
#include <camera/client_video_camera.h>
#include <camera/resource_display.h>

#include <ui/actions/action_manager.h>
#include <ui/actions/action_parameter_types.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/controls/time_slider.h>
#include <ui/graphics/items/controls/time_scroll_bar.h>
#include <ui/graphics/instruments/signaling_instrument.h>
#include <ui/widgets/calendar_widget.h>
#include <ui/widgets/day_time_widget.h>
#include <ui/widgets/search_line_edit.h>

#include "extensions/workbench_stream_synchronizer.h"
#include "watchers/workbench_server_time_watcher.h"
#include "watchers/workbench_user_inactivity_watcher.h"
#include "workbench.h"
#include "workbench_display.h"
#include "workbench_context.h"
#include "workbench_item.h"
#include "workbench_layout.h"
#include "workbench_layout_snapshot_manager.h"

#include "camera/thumbnails_loader.h"
#include "plugins/resource/archive/abstract_archive_stream_reader.h"
#include "redass/redass_controller.h"

QnWorkbenchNavigator::QnWorkbenchNavigator(QObject *parent):
    base_type(parent),
    QnWorkbenchContextAware(parent),
    m_streamSynchronizer(context()->instance<QnWorkbenchStreamSynchronizer>()),
    m_timeSlider(NULL),
    m_timeScrollBar(NULL),
    m_calendar(NULL),
    m_dayTimeWidget(NULL),
    m_bookmarksSearchWidget(NULL),
    m_centralWidget(NULL),
    m_currentWidget(NULL),
    m_currentMediaWidget(NULL),
    m_currentWidgetFlags(0),
    m_currentWidgetLoaded(false),
    m_sliderDataInvalid(false),
    m_updatingSliderFromReader(false),
    m_updatingSliderFromScrollBar(false),
    m_updatingScrollBarFromSlider(false),
    m_lastLive(false),
    m_lastLiveSupported(false),
    m_lastPlaying(false),
    m_lastPlayingSupported(false),
    m_pausedOverride(false),
    m_preciseNextSeek(false),
    m_autoPaused(false),
    m_lastSpeed(0.0),
    m_lastMinimalSpeed(0.0),
    m_lastMaximalSpeed(0.0),
    m_lastUpdateSlider(0),
    m_lastCameraTime(0),
    m_startSelectionAction(new QAction(this)),
    m_endSelectionAction(new QAction(this)),
    m_clearSelectionAction(new QAction(this))
    , m_cameraDataManager(NULL)
{
    /* We'll be using this one, so make sure it's created. */
    context()->instance<QnWorkbenchServerTimeWatcher>();
    m_updateSliderTimer.restart();

    m_cameraDataManager = context()->instance<QnCameraDataManager>();
    connect(m_cameraDataManager, &QnCameraDataManager::periodsChanged, this, &QnWorkbenchNavigator::updateLoaderPeriods);
    connect(m_cameraDataManager, &QnCameraDataManager::bookmarksChanged, this, &QnWorkbenchNavigator::updateLoaderBookmarks);

    //TODO: #GDM Temporary fix for the Feature #4714. Correct change would be: expand getTimePeriods query with Region data,
    // then truncate cached chunks by this region and synchronize the cache.
    QTimer* discardCacheTimer = new QTimer(this);
    discardCacheTimer->setInterval(10*60*1000);
    discardCacheTimer->setSingleShot(false);
    connect(discardCacheTimer, &QTimer::timeout, m_cameraDataManager, &QnCameraDataManager::clearCache);
    discardCacheTimer->start();
}
    
QnWorkbenchNavigator::~QnWorkbenchNavigator() {
    foreach(QnThumbnailsLoader *loader, m_thumbnailLoaderByResource)
        QnRunnableCleanup::cleanup(loader);
}

QnTimeSlider *QnWorkbenchNavigator::timeSlider() const {
    return m_timeSlider;
}

void QnWorkbenchNavigator::setTimeSlider(QnTimeSlider *timeSlider) {
    if(m_timeSlider == timeSlider)
        return;

    if(m_timeSlider) {
        disconnect(m_timeSlider, NULL, this, NULL);
        
        if(isValid())
            deinitialize();
    }

    m_timeSlider = timeSlider;

    if(m_timeSlider) {
        connect(m_timeSlider, &QObject::destroyed, this, [this](){setTimeSlider(NULL);});

        if(isValid())
            initialize();
    }
}

QnTimeScrollBar *QnWorkbenchNavigator::timeScrollBar() const {
    return m_timeScrollBar;
}

void QnWorkbenchNavigator::setTimeScrollBar(QnTimeScrollBar *scrollBar) {
    if(m_timeScrollBar == scrollBar)
        return;

    if(m_timeScrollBar) {
        disconnect(m_timeScrollBar, NULL, this, NULL);

        if(isValid())
            deinitialize();
    }

    m_timeScrollBar = scrollBar;

    if(m_timeScrollBar) {
        connect(m_timeScrollBar, &QObject::destroyed, this, [this](){setTimeScrollBar(NULL);});

        if(isValid())
            initialize();
    }
}

QnCalendarWidget *QnWorkbenchNavigator::calendar() const{
    return m_calendar;
}

void QnWorkbenchNavigator::setCalendar(QnCalendarWidget *calendar){
    if(m_calendar == calendar)
        return;

    if(m_calendar) {
        disconnect(m_calendar, NULL, this, NULL);

        if(isValid())
            deinitialize();
    }

    m_calendar = calendar;

    if(m_calendar) {
        connect(m_calendar, &QObject::destroyed, this, [this](){setCalendar(NULL);});

        if(isValid())
            initialize();
    }
}

QnDayTimeWidget *QnWorkbenchNavigator::dayTimeWidget() const {
    return m_dayTimeWidget;
}

void QnWorkbenchNavigator::setDayTimeWidget(QnDayTimeWidget *dayTimeWidget) {
    if(m_dayTimeWidget == dayTimeWidget)
        return;

    if(m_dayTimeWidget) {
        disconnect(m_dayTimeWidget, NULL, this, NULL);

        if(isValid())
            deinitialize();
    }

    m_dayTimeWidget = dayTimeWidget;

    if(m_dayTimeWidget) {
        connect(m_dayTimeWidget, &QObject::destroyed, this, [this](){setDayTimeWidget(NULL);});

        if(isValid())
            initialize();
    }
}

bool QnWorkbenchNavigator::isValid() {
    return m_timeSlider && m_timeScrollBar && m_calendar && m_bookmarksSearchWidget;
}

void QnWorkbenchNavigator::initialize() {
    assert(isValid());

    connect(workbench(),                        SIGNAL(currentLayoutChanged()),                     this,   SLOT(updateSliderOptions()));

    connect(display(),                          SIGNAL(widgetChanged(Qn::ItemRole)),                this,   SLOT(at_display_widgetChanged(Qn::ItemRole)));
    connect(display(),                          SIGNAL(widgetAdded(QnResourceWidget *)),            this,   SLOT(at_display_widgetAdded(QnResourceWidget *)));
    connect(display(),                          SIGNAL(widgetAboutToBeRemoved(QnResourceWidget *)), this,   SLOT(at_display_widgetAboutToBeRemoved(QnResourceWidget *)));
    connect(display()->beforePaintInstrument(), SIGNAL(activated(QWidget *, QEvent *)),             this,   SLOT(updateSliderFromReader()));

    connect(m_timeSlider,                       SIGNAL(valueChanged(qint64)),                       this,   SLOT(at_timeSlider_valueChanged(qint64)));
    connect(m_timeSlider,                       SIGNAL(sliderPressed()),                            this,   SLOT(at_timeSlider_sliderPressed()));
    connect(m_timeSlider,                       SIGNAL(sliderReleased()),                           this,   SLOT(at_timeSlider_sliderReleased()));
    connect(m_timeSlider,                       SIGNAL(valueChanged(qint64)),                       this,   SLOT(updateScrollBarFromSlider()));
    connect(m_timeSlider,                       SIGNAL(rangeChanged(qint64, qint64)),               this,   SLOT(updateScrollBarFromSlider()));
    connect(m_timeSlider,                       SIGNAL(windowChanged(qint64, qint64)),              this,   SLOT(updateScrollBarFromSlider()));
    connect(m_timeSlider,                       SIGNAL(windowChanged(qint64, qint64)),              this,   SLOT(updateTargetPeriod()));
    connect(m_timeSlider,                       SIGNAL(windowChanged(qint64, qint64)),              this,   SLOT(updateCalendarFromSlider()));
    connect(m_timeSlider,                       SIGNAL(selectionReleased()),                        this,   SLOT(at_timeSlider_selectionReleased()));
    connect(m_timeSlider,                       SIGNAL(customContextMenuRequested(const QPointF &, const QPoint &)), this, SLOT(at_timeSlider_customContextMenuRequested(const QPointF &, const QPoint &)));
    connect(m_timeSlider,                       SIGNAL(selectionPressed()),                         this,   SLOT(at_timeSlider_selectionPressed()));
    connect(m_timeSlider,                       SIGNAL(thumbnailsVisibilityChanged()),              this,   SLOT(updateTimeSliderWindowSizePolicy()));
    connect(m_timeSlider,                       SIGNAL(thumbnailClicked()),                         this,   SLOT(at_timeSlider_thumbnailClicked()));
    m_timeSlider->setLineCount(SliderLineCount);
    m_timeSlider->setLineStretch(CurrentLine, 1.5);
    m_timeSlider->setLineStretch(SyncedLine, 1.0);
    m_timeSlider->setRange(0, 1000ll * 60 * 60 * 24);
    m_timeSlider->setWindow(m_timeSlider->minimum(), m_timeSlider->maximum());

    connect(m_timeScrollBar,                    SIGNAL(valueChanged(qint64)),                       this,   SLOT(updateSliderFromScrollBar()));
    connect(m_timeScrollBar,                    SIGNAL(pageStepChanged(qint64)),                    this,   SLOT(updateSliderFromScrollBar()));
    connect(m_timeScrollBar,                    SIGNAL(sliderPressed()),                            this,   SLOT(at_timeScrollBar_sliderPressed()));
    connect(m_timeScrollBar,                    SIGNAL(sliderReleased()),                           this,   SLOT(at_timeScrollBar_sliderReleased()));
    m_timeScrollBar->installEventFilter(this);

    connect(m_calendar,                         SIGNAL(dateClicked(const QDate &)),                 this,   SLOT(at_calendar_dateClicked(const QDate &)));
    connect(m_calendar,                         SIGNAL(currentPageChanged(int,int)),                this,   SLOT(updateTargetPeriod()));

    connect(m_dayTimeWidget,                    SIGNAL(timeClicked(const QTime &)),                 this,   SLOT(at_dayTimeWidget_timeClicked(const QTime &)));

    connect(m_bookmarksSearchWidget, &QnSearchLineEdit::textChanged, this, [this](const QString &text) {
        if (!m_currentMediaWidget)
            return;
        //TODO: #GDM #Bookmarks do not search till the full tag or at least 3 letters will be entered, search once in 2-3 seconds
        if(QnCachingCameraDataLoader *loader = loaderByWidget(m_currentMediaWidget))
            loader->setBookmarksTextFilter(text); //TODO: #GDM #Bookmarks synced widgets? clear previous?
    });

    connect(context()->instance<QnWorkbenchServerTimeWatcher>(), SIGNAL(offsetsChanged()),          this,   SLOT(updateLocalOffset()));
    connect(qnSettings->notifier(QnClientSettings::TIME_MODE), SIGNAL(valueChanged(int)),           this,   SLOT(updateLocalOffset()));

    connect(context()->instance<QnWorkbenchUserInactivityWatcher>(),    SIGNAL(stateChanged(bool)), this,   SLOT(setAutoPaused(bool)));

    updateLines();
    updateCalendar();
    updateScrollBarFromSlider();
    updateTimeSliderWindowSizePolicy();
} 

void QnWorkbenchNavigator::deinitialize() {
    assert(isValid());

    disconnect(workbench(),                         NULL, this, NULL);

    disconnect(display(),                           NULL, this, NULL);
    disconnect(display()->beforePaintInstrument(),  NULL, this, NULL);
    
    disconnect(m_timeSlider,                        NULL, this, NULL);

    disconnect(m_timeScrollBar,                     NULL, this, NULL);
    m_timeScrollBar->removeEventFilter(this);

    disconnect(m_calendar,                          NULL, this, NULL);

    disconnect(context()->instance<QnWorkbenchServerTimeWatcher>(), NULL, this, NULL);
    disconnect(qnSettings->notifier(QnClientSettings::TIME_MODE), NULL, this, NULL);

    m_currentWidget = NULL;
    m_currentWidgetFlags = 0;
}

Qn::ActionScope QnWorkbenchNavigator::currentScope() const {
    return Qn::SliderScope;
}

QVariant QnWorkbenchNavigator::currentTarget(Qn::ActionScope scope) const {
    if(scope != Qn::SliderScope)
        return QVariant();

    QnResourceWidgetList result;
    if(m_currentWidget)
        result.push_back(m_currentWidget);
    return QVariant::fromValue<QnResourceWidgetList>(result);
}

bool QnWorkbenchNavigator::isLiveSupported() const {
    return m_currentWidgetFlags & WidgetSupportsLive;
}

bool QnWorkbenchNavigator::isLive() const {
    return isLiveSupported() && m_timeSlider->value() == m_timeSlider->maximum();
}

bool QnWorkbenchNavigator::setLive(bool live) {
    if(live == isLive())
        return true;

    if(!isLiveSupported())
        return false;

    if (live) {
        m_timeSlider->setValue(m_timeSlider->maximum(), true);
    } else {
        m_timeSlider->setValue(m_timeSlider->minimum(), true); // TODO: #Elric need to save position here.
    }
    return true;
}

bool QnWorkbenchNavigator::isPlayingSupported() const {
    if (m_currentMediaWidget) 
        return m_currentMediaWidget->display()->archiveReader();
    return false;
}

bool QnWorkbenchNavigator::isPlaying() const {
    if(!isPlayingSupported())
        return false;
    if (!m_currentMediaWidget)
        return false;

    return !m_currentMediaWidget->display()->archiveReader()->isMediaPaused();
}

bool QnWorkbenchNavigator::setPlaying(bool playing) {
    if(playing == isPlaying())
        return true;
    
    if (!m_currentMediaWidget)
        return false;

    if(!isPlayingSupported())
        return false;

    if (playing && m_autoPaused)
        return false;

    m_pausedOverride = false;
    m_autoPaused = false;

    QnAbstractArchiveReader *reader = m_currentMediaWidget->display()->archiveReader();
    QnCamDisplay *camDisplay = m_currentMediaWidget->display()->camDisplay();
    if(playing) {
        if (reader->isRealTimeSource()) {
            /* Media was paused while on live. Jump to archive when resumed. */
            qint64 time = camDisplay->getCurrentTime();
            reader->resumeMedia();
            if (time != (qint64)AV_NOPTS_VALUE && !reader->isReverseMode())
                reader->directJumpToNonKeyFrame(time+1);
        } else {
            reader->resumeMedia();
        }
        camDisplay->playAudio(true);

        if(qFuzzyIsNull(speed()))
            setSpeed(1.0);
    } else {
        reader->pauseMedia();
        setSpeed(0.0);
    }

    updatePlaying();
    return true;
}

qreal QnWorkbenchNavigator::speed() const {
    if(!m_currentMediaWidget || m_currentMediaWidget->display()->isStillImage())
        return 0.0;

    if(QnAbstractArchiveReader *reader = m_currentMediaWidget->display()->archiveReader())
        return reader->getSpeed();

    return 1.0;
}

void QnWorkbenchNavigator::setSpeed(qreal speed) {
    speed = qBound(minimalSpeed(), speed, maximalSpeed());
    if(qFuzzyCompare(speed, this->speed()))
        return;

    if(!m_currentMediaWidget)
        return;

    if(QnAbstractArchiveReader *reader = m_currentMediaWidget->display()->archiveReader()) {
        reader->setSpeed(speed);

        setPlaying(!qFuzzyIsNull(speed));

        updateSpeed();
    }
}

qreal QnWorkbenchNavigator::minimalSpeed() const {
    if(!m_currentMediaWidget || m_currentMediaWidget->display()->isStillImage())
        return 0.0;

    if(QnAbstractArchiveReader *reader = m_currentMediaWidget->display()->archiveReader())
        return reader->isNegativeSpeedSupported() ? -16.0 : 0.0;

    return 1.0;
}

qreal QnWorkbenchNavigator::maximalSpeed() const {
    if(!m_currentMediaWidget || m_currentMediaWidget->display()->isStillImage())
        return 0.0;

    if(m_currentMediaWidget->display()->archiveReader() != NULL)
        return 16.0;

    return 1.0;
}

qint64 QnWorkbenchNavigator::position() const {
    if(!m_currentMediaWidget)
        return 0;

    return m_currentMediaWidget->display()->camera()->getCurrentTime();
}

void QnWorkbenchNavigator::setPosition(qint64 position) {
    if(!m_currentMediaWidget)
        return;

    QnAbstractArchiveReader *reader = m_currentMediaWidget->display()->archiveReader();
    if(!reader)
        return;

    if(isPlaying())
        reader->jumpTo(position, 0);
    else
        reader->jumpTo(position, position);
    emit positionChanged();
}

void QnWorkbenchNavigator::addSyncedWidget(QnMediaResourceWidget *widget) {
    if (widget == NULL) {
        qnNullWarning(widget);
        return;
    }

    m_syncedWidgets.insert(widget);
    m_syncedResources.insert(widget->resource()->toResourcePtr(), QHashDummyValue());

    updateCurrentWidget();
    updateSyncedPeriods();
}

void QnWorkbenchNavigator::removeSyncedWidget(QnMediaResourceWidget *widget) {
    if(!m_syncedWidgets.remove(widget))
        return;

    if (display() && !display()->isChangingLayout()){
        if(m_syncedWidgets.contains(m_currentMediaWidget))
            updateItemDataFromSlider(widget);
    }

    /* QHash::erase does nothing when called for container's end, 
     * and is therefore perfectly safe. */
    m_syncedResources.erase(m_syncedResources.find(widget->resource()->toResourcePtr()));
    m_motionIgnoreWidgets.remove(widget);

    if(QnCachingCameraDataLoader *loader = loaderByWidget(widget))
        loader->setMotionRegions(QList<QRegion>());

    updateCurrentWidget();
    updateSyncedPeriods();
}

QnResourceWidget *QnWorkbenchNavigator::currentWidget() const {
    return m_currentWidget;
}

QnWorkbenchNavigator::WidgetFlags QnWorkbenchNavigator::currentWidgetFlags() const {
    return m_currentWidgetFlags;
}

void QnWorkbenchNavigator::updateItemDataFromSlider(QnResourceWidget *widget) const {
    if(!widget)
        return;

    QnWorkbenchItem *item = widget->item();

    QnTimePeriod window(m_timeSlider->windowStart(), m_timeSlider->windowEnd() - m_timeSlider->windowStart());
    if(m_timeSlider->windowEnd() == m_timeSlider->maximum()) // TODO: #Elric check that widget supports live.
        window.durationMs = -1;
    item->setData(Qn::ItemSliderWindowRole, QVariant::fromValue<QnTimePeriod>(window));

    QnTimePeriod selection;
    if(m_timeSlider->isSelectionValid())
        selection = QnTimePeriod(m_timeSlider->selectionStart(), m_timeSlider->selectionEnd() - m_timeSlider->selectionStart());
    item->setData(Qn::ItemSliderSelectionRole, QVariant::fromValue<QnTimePeriod>(selection));
}

void QnWorkbenchNavigator::updateSliderFromItemData(QnResourceWidget *widget, bool preferToPreserveWindow) {
    if(!widget)
        return;

    QnWorkbenchItem *item = widget->item();

    QnTimePeriod window = item->data(Qn::ItemSliderWindowRole).value<QnTimePeriod>();
    QnTimePeriod selection = item->data(Qn::ItemSliderSelectionRole).value<QnTimePeriod>();

    if(preferToPreserveWindow && m_timeSlider->value() >= m_timeSlider->windowStart() && m_timeSlider->value() <= m_timeSlider->windowEnd()) {
        /* Just skip window initialization. */
    } else {
        if(window.isEmpty())
            window = QnTimePeriod(0, -1);

        qint64 windowStart = window.startTimeMs;
        qint64 windowEnd = window.durationMs == -1 ? m_timeSlider->maximum() : window.startTimeMs + window.durationMs;
        if(windowStart < m_timeSlider->minimum()) {
            qint64 delta = m_timeSlider->minimum() - windowStart;
            windowStart += delta;
            windowEnd += delta;
        }
        m_timeSlider->setWindow(windowStart, windowEnd);
    }

    m_timeSlider->setSelectionValid(!selection.isNull());
    m_timeSlider->setSelection(selection.startTimeMs, selection.startTimeMs + selection.durationMs);
}

QnThumbnailsLoader *QnWorkbenchNavigator::thumbnailLoader(const QnResourcePtr &resource) {
    QHash<QnResourcePtr, QnThumbnailsLoader *>::const_iterator pos = m_thumbnailLoaderByResource.find(resource);
    if(pos != m_thumbnailLoaderByResource.end())
        return *pos;

    QnThumbnailsLoader *loader = new QnThumbnailsLoader(resource);

    m_thumbnailLoaderByResource[resource] = loader;
    return loader;
}

QnThumbnailsLoader *QnWorkbenchNavigator::thumbnailLoader(QnResourceWidget *widget) {
    return widget ? thumbnailLoader(widget->resource()) : NULL;
}

void QnWorkbenchNavigator::jumpBackward() {
    if(!m_currentMediaWidget)
        return;

    QnAbstractArchiveReader *reader = m_currentMediaWidget->display()->archiveReader();
    if(!reader)
        return;

    m_pausedOverride = false;

    qint64 pos = reader->startTime();
    if(QnCachingCameraDataLoader *loader = loaderByWidget(m_currentMediaWidget)) {
        bool canUseMotion = m_currentWidget->options() & QnResourceWidget::DisplayMotion;
        QnTimePeriodList periods = loader->periods(loader->isMotionRegionsEmpty() || !canUseMotion ? Qn::RecordingContent : Qn::MotionContent);
        if (loader->isMotionRegionsEmpty())
            periods = QnTimePeriodList::aggregateTimePeriods(periods, MAX_FRAME_DURATION);
        
        if (!periods.isEmpty()) {
            qint64 currentTime = m_currentMediaWidget->display()->camera()->getCurrentTime();

            if (currentTime == DATETIME_NOW) {
                pos = periods.last().startTimeMs * 1000;
            } else {
                QnTimePeriodList::const_iterator itr = periods.findNearestPeriod(currentTime/1000, true);
                itr = qMax(itr - 1, periods.constBegin());
                pos = itr->startTimeMs * 1000;
                if (reader->isReverseMode() && itr->durationMs != -1)
                    pos += itr->durationMs * 1000;
            }
        }
    }
    reader->jumpTo(pos, pos);
    emit positionChanged();
}

void QnWorkbenchNavigator::jumpForward() {
    if (!m_currentMediaWidget)
        return;

    QnAbstractArchiveReader *reader = m_currentMediaWidget->display()->archiveReader();
    if(!reader)
        return;

    m_pausedOverride = false;

    qint64 pos;
    if(!(m_currentWidgetFlags & WidgetSupportsPeriods)) {
        pos = reader->endTime();
    } else if (QnCachingCameraDataLoader *loader = loaderByWidget(m_currentMediaWidget)) {
        bool canUseMotion = m_currentWidget->options() & QnResourceWidget::DisplayMotion;
        QnTimePeriodList periods = loader->periods(loader->isMotionRegionsEmpty() || !canUseMotion ? Qn::RecordingContent : Qn::MotionContent);
        if (loader->isMotionRegionsEmpty())
            periods = QnTimePeriodList::aggregateTimePeriods(periods, MAX_FRAME_DURATION);

        qint64 currentTime = m_currentMediaWidget->display()->camera()->getCurrentTime() / 1000;
        QnTimePeriodList::const_iterator itr = periods.findNearestPeriod(currentTime, true);
        if (itr != periods.constEnd())
            ++itr;
        
        if (itr == periods.constEnd()) {
            /* Do not make step forward to live if we are playing backward. */
            if (reader->isReverseMode())
                return;
            pos = DATETIME_NOW; 
        } else if (reader->isReverseMode() && itr->durationMs == -1) {
            /* Do not make step forward to live if we are playing backward. */
            --itr;
            pos = itr->startTimeMs * 1000;
        } else {
            pos = (itr->startTimeMs + (reader->isReverseMode() ? itr->durationMs : 0)) * 1000;
        }
    }
    reader->jumpTo(pos, pos);
    emit positionChanged();
}

void QnWorkbenchNavigator::stepBackward() {
    if(!m_currentMediaWidget)
        return;

    QnAbstractArchiveReader *reader = m_currentMediaWidget->display()->archiveReader();
    if(!reader)
        return;

    m_pausedOverride = false;

    qint64 currentTime = m_currentMediaWidget->display()->camera()->getCurrentTime();
    if (!reader->isSkippingFrames() && currentTime > reader->startTime() && !m_currentMediaWidget->display()->camDisplay()->isBuffering()) {

        if (reader->isSingleShotMode())
            m_currentMediaWidget->display()->camDisplay()->playAudio(false); // TODO: #Elric wtf?

        reader->previousFrame(currentTime);
    }
    emit positionChanged();
}

void QnWorkbenchNavigator::stepForward() {
    if(!m_currentMediaWidget)
        return;

    QnAbstractArchiveReader *reader = m_currentMediaWidget->display()->archiveReader();
    if(!reader)
        return;

    m_pausedOverride = false;

    reader->nextFrame();
    emit positionChanged();
}

void QnWorkbenchNavigator::setPlayingTemporary(bool playing) {
    if (!m_currentMediaWidget)
        return;

    if (playing)
        m_currentMediaWidget->display()->archiveReader()->resumeMedia();
    else
        m_currentMediaWidget->display()->archiveReader()->pauseMedia();
    m_currentMediaWidget->display()->camDisplay()->playAudio(playing);
}

// -------------------------------------------------------------------------- //
// Updaters
// -------------------------------------------------------------------------- //
void QnWorkbenchNavigator::updateCentralWidget() {
    QnResourceWidget *centralWidget = display()->widget(Qn::CentralRole);
    if(m_centralWidget == centralWidget)
        return;

    m_centralWidget = centralWidget;

    updateCurrentWidget();
    updateThumbnailsLoader();
}

void QnWorkbenchNavigator::updateCurrentWidget() {
    QnResourceWidget *widget = m_centralWidget;
    if (m_currentWidget == widget)
        return;
    
    QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget);

    emit currentWidgetAboutToBeChanged();

    WidgetFlags previousWidgetFlags = m_currentWidgetFlags;

    if(m_currentWidget) {
        if(m_streamSynchronizer->isRunning() && (m_currentWidgetFlags & WidgetSupportsPeriods))
            foreach(QnResourceWidget *widget, m_syncedWidgets)
                updateItemDataFromSlider(widget); //TODO: #GDM #Common ask #elric: should it be done at every selection change?
        else
            updateItemDataFromSlider(m_currentWidget);
        disconnect(m_currentWidget->resource(), NULL, this, NULL);
    } else {
        m_sliderDataInvalid = true;
        m_sliderWindowInvalid = true;
    }

    if (display() && display()->isChangingLayout()) {
        // clear current widget state to avoid incorrect behavior when closing the layout
        // see: Bug #1341: Selection on timeline aren't displayed after thumbnails searching
        // see: Bug #1344: If make a THMB search from a layout with a result of THMB search, Timeline are not marked properly
        m_currentWidget = NULL;
        m_currentMediaWidget = NULL;
    } else {
        m_currentWidget = widget;
        m_currentMediaWidget = mediaWidget;
    }

    if (m_currentWidget)
        connect(m_currentWidget->resource(), &QnResource::nameChanged, this, &QnWorkbenchNavigator::updateLines);

    m_pausedOverride = false;
    m_currentWidgetLoaded = false;

    updateCurrentWidgetFlags();
    updateLines();
    updateCalendar();

    if(!((m_currentWidgetFlags & WidgetSupportsSync) && (previousWidgetFlags & WidgetSupportsSync) && m_streamSynchronizer->isRunning()) && m_currentWidget) {
        m_sliderDataInvalid = true;
        m_sliderWindowInvalid |= (m_currentWidgetFlags & WidgetUsesUTC) != (previousWidgetFlags & WidgetUsesUTC);
    }
    updateSliderFromReader(false);
    m_timeSlider->finishAnimations();

    if(m_currentMediaWidget) {
        QMetaObject::invokeMethod(this, "updatePlaying", Qt::QueuedConnection); // TODO: #Elric evil hacks...
        QMetaObject::invokeMethod(this, "updateSpeed", Qt::QueuedConnection);
    }

    action(Qn::ToggleBookmarksSearchAction)->setEnabled(m_currentMediaWidget && m_currentWidget->resource()->flags() & Qn::utc);

    updateLocalOffset();
    updateCurrentPeriods();
    updateCurrentBookmarks();
    updateLiveSupported();
    updateLive();
    updatePlayingSupported();
    updatePlaying();
    updateSpeedRange();
    updateSpeed();

    emit currentWidgetChanged();
}

void QnWorkbenchNavigator::updateLocalOffset() {
    qint64 localOffset = 0;
    if(qnSettings->timeMode() == Qn::ServerTimeMode && m_currentMediaWidget && (m_currentWidgetFlags & WidgetUsesUTC))
        localOffset = context()->instance<QnWorkbenchServerTimeWatcher>()->localOffset(m_currentMediaWidget->resource(), 0);
    m_timeSlider->setLocalOffset(localOffset);
    m_calendar->setLocalOffset(localOffset);
    m_dayTimeWidget->setLocalOffset(localOffset);
}

void QnWorkbenchNavigator::updateCurrentWidgetFlags() {
    WidgetFlags flags = 0;

    if(m_currentWidget) {
        flags = 0;

        if(m_currentWidget->resource().dynamicCast<QnSecurityCamResource>())
            flags |= WidgetSupportsLive | WidgetSupportsPeriods;

        if(m_currentWidget->resource()->flags() & Qn::periods)
            flags |= WidgetSupportsPeriods;

        if(m_currentWidget->resource()->flags() & Qn::utc)
            flags |= WidgetUsesUTC;

        if(m_currentWidget->resource()->flags() & Qn::sync)
            flags |= WidgetSupportsSync;

        QnThumbnailsSearchState searchState = workbench()->currentLayout()->data(Qn::LayoutSearchStateRole).value<QnThumbnailsSearchState>();
        if(searchState.step > 0) /* Is a thumbnails search layout. */
            flags &= ~(WidgetSupportsLive | WidgetSupportsSync);

        QnTimePeriod period = workbench()->currentLayout()->resource() ? workbench()->currentLayout()->resource()->getLocalRange() : QnTimePeriod();
        if(!period.isNull())
            flags &= ~WidgetSupportsLive;
    } else {
        flags = 0;
    }

    if(m_currentWidgetFlags == flags)
        return;

    m_currentWidgetFlags = flags;
    
    updateSliderOptions();
}

void QnWorkbenchNavigator::updateSliderOptions() {
    m_timeSlider->setOption(QnTimeSlider::UseUTC, m_currentWidgetFlags & WidgetUsesUTC);

    bool selectionEditable = workbench()->currentLayout()->resource(); //&& (workbench()->currentLayout()->resource()->flags() & Qn::local_media) != Qn::local_media;
    m_timeSlider->setOption(QnTimeSlider::SelectionEditable, selectionEditable);
    if(!selectionEditable)
        m_timeSlider->setSelectionValid(false);
}

void QnWorkbenchNavigator::updateSliderFromReader(bool keepInWindow) {
    if (!m_currentMediaWidget)
        return;

    if (m_timeSlider->isSliderDown())
        return;

    QnAbstractArchiveReader *reader = m_currentMediaWidget->display()->archiveReader();
    if (!reader)
        return;
#ifdef Q_OS_MAC
    // todo: MAC  got stuck in full screen mode if update slider to often! #elric: refactor it!
    if (m_updateSliderTimer.elapsed() < 33)
        return;
    m_updateSliderTimer.restart();
#endif

    QN_SCOPED_VALUE_ROLLBACK(&m_updatingSliderFromReader, true);

    QnThumbnailsSearchState searchState = workbench()->currentLayout()->data(Qn::LayoutSearchStateRole).value<QnThumbnailsSearchState>();
    bool isSearch = searchState.step > 0;

    qint64 startTimeUSec, endTimeUSec;
    qint64 endTimeMSec, startTimeMSec;
    if(isSearch) {
        endTimeMSec = searchState.period.endTimeMs();
        endTimeUSec = endTimeMSec * 1000;
        startTimeMSec = searchState.period.startTimeMs;
        startTimeUSec = startTimeMSec * 1000;
    } else {
        endTimeUSec = reader->endTime();
        endTimeMSec = endTimeUSec == DATETIME_NOW ? qnSyncTime->currentMSecsSinceEpoch() : ((quint64)endTimeUSec == AV_NOPTS_VALUE ? m_timeSlider->maximum() : endTimeUSec / 1000);

        startTimeUSec = reader->startTime();                       
        startTimeMSec = startTimeUSec == DATETIME_NOW 
            ? endTimeMSec - 10000                               /* If nothing is recorded, set minimum to end - 10s. */        
            : (quint64)startTimeUSec == AV_NOPTS_VALUE 
            ? m_timeSlider->minimum() 
            : startTimeUSec / 1000;
    }

    m_timeSlider->setRange(startTimeMSec, endTimeMSec);
    if(m_calendar)
        m_calendar->setDateRange(QDateTime::fromMSecsSinceEpoch(startTimeMSec).date(), QDateTime::fromMSecsSinceEpoch(endTimeMSec).date());
    if(m_dayTimeWidget)
        m_dayTimeWidget->setEnabledWindow(startTimeMSec, endTimeMSec);

    if(!m_pausedOverride) {
        qint64 timeUSec = m_currentMediaWidget->display()->camDisplay()->isRealTimeSource() ? DATETIME_NOW : m_currentMediaWidget->display()->camera()->getCurrentTime();
        if ((quint64)timeUSec == AV_NOPTS_VALUE)
            timeUSec = -1;

        if(isSearch && timeUSec < 0) {
            timeUSec = m_currentMediaWidget->item()->data<qint64>(Qn::ItemTimeRole, -1);
            if (timeUSec != DATETIME_NOW && timeUSec >= 0)
                timeUSec *= 1000;
        }
        qint64 timeMSec = timeUSec == DATETIME_NOW ? endTimeMSec : (timeUSec < 0 ? m_timeSlider->value() : timeUSec / 1000);
        qint64 timeNext = m_currentMediaWidget->display()->camDisplay()->isRealTimeSource() ? AV_NOPTS_VALUE : m_currentMediaWidget->display()->camDisplay()->getNextTime();

        if (timeUSec != DATETIME_NOW && timeUSec >= 0) {
            qint64 now = QDateTime::currentDateTimeUtc().toMSecsSinceEpoch();
            if (m_lastUpdateSlider && m_lastCameraTime == timeMSec && (quint64)timeNext != AV_NOPTS_VALUE && timeNext - timeMSec <= MAX_FRAME_DURATION){
                qint64 timeDiff = (now - m_lastUpdateSlider) * speed();
                if (timeDiff < MAX_FRAME_DURATION)
                    timeMSec += timeDiff;
            } else {
                m_lastCameraTime = timeMSec;
                m_lastUpdateSlider = now;
            }
        }

        m_timeSlider->setValue(timeMSec, keepInWindow);

        if(timeUSec >= 0)
            updateLive();

        bool sync = (m_streamSynchronizer->isRunning() && (m_currentWidgetFlags & WidgetSupportsPeriods));
        if(isSearch || !sync) {
            QVector<qint64> indicators;
            foreach(QnResourceWidget *widget, display()->widgets())
                if(QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget))
                    if (mediaWidget != m_currentMediaWidget && mediaWidget->resource()->toResource()->hasFlags(Qn::sync))
                        indicators.push_back(mediaWidget->display()->camera()->getCurrentTime() / 1000);
            m_timeSlider->setIndicators(indicators);
        } else {
            m_timeSlider->setIndicators(QVector<qint64>());
        }
    }

    if(!m_currentWidgetLoaded && ((quint64)startTimeUSec != AV_NOPTS_VALUE && (quint64)endTimeUSec != AV_NOPTS_VALUE)) {
        m_currentWidgetLoaded = true;
        updateTargetPeriod();

        if(m_sliderDataInvalid) {
            updateSliderFromItemData(m_currentMediaWidget, !m_sliderWindowInvalid);
            m_timeSlider->finishAnimations();
            m_sliderDataInvalid = false;
            m_sliderWindowInvalid = false;
        }
    }
}

void QnWorkbenchNavigator::updateTargetPeriod() {
    if (!m_currentWidgetLoaded) 
        return;

    /* Do not update periods if lines are hidden. */
    if (!(m_currentWidgetFlags & WidgetSupportsPeriods))
        return;

    /* Update target time period for time period loaders. */
    QnTimePeriod timeSliderPeriod(m_timeSlider->windowStart(), m_timeSlider->windowEnd() - m_timeSlider->windowStart());
    QnTimePeriod boundingPeriod(m_timeSlider->minimum(), m_timeSlider->maximum() - m_timeSlider->minimum());
    //TODO: #Elric Invalid boundingPeriod near daylight time
    boundingPeriod = boundingPeriod.intersected(QnTimePeriod(0, qnSyncTime->currentMSecsSinceEpoch()));

    /* Some time periods should also be displayed on the calendar so we are loading them for the whole month. */
    QnTimePeriod calendarPeriod(timeSliderPeriod);
    if (m_calendar) {
        QDate date(m_calendar->yearShown(), m_calendar->monthShown(), 1);
        QnTimePeriod monthPeriod(QDateTime(date).toMSecsSinceEpoch(), QDateTime(date.addMonths(1)).toMSecsSinceEpoch() - QDateTime(date).toMSecsSinceEpoch());
        calendarPeriod.addPeriod(boundingPeriod.intersected(monthPeriod));
    }

    QSet<QnMediaResourceWidget*> widgets = m_syncedWidgets;
    if (m_currentMediaWidget)
        widgets.insert(m_currentMediaWidget);

    foreach(QnMediaResourceWidget *widget, widgets) {
        QnCachingCameraDataLoader *loader = loaderByWidget(widget);
        if (!loader)
            continue;

        loader->setBoundingPeriod(boundingPeriod);
        for (int i = 0; i < Qn::CameraDataTypeCount; ++i) {
            Qn::CameraDataType dataType = static_cast<Qn::CameraDataType>(i);
            switch (dataType) {
            case Qn::RecordedTimePeriod:
            case Qn::MotionTimePeriod:
                loader->setTargetPeriod(calendarPeriod, dataType);
                break;
            case Qn::BookmarkTimePeriod:
            case Qn::BookmarkData:
                loader->setTargetPeriod(timeSliderPeriod, dataType);
                break;
            default:
                break;
            }
        }

    }
}

void QnWorkbenchNavigator::updateCurrentPeriods() {
    for(int i = 0; i < Qn::TimePeriodContentCount; i++)
        updateCurrentPeriods(static_cast<Qn::TimePeriodContent>(i));
}

void QnWorkbenchNavigator::updateCurrentPeriods(Qn::TimePeriodContent type) {
    QnTimePeriodList periods;

    if (type == Qn::MotionContent && m_currentWidget && !(m_currentWidget->options() & QnResourceWidget::DisplayMotion)) {
        /* Use empty periods. */
    } else if(QnCachingCameraDataLoader *loader = loaderByWidget(m_currentWidget)) {
        periods = loader->periods(type);
    }

    m_timeSlider->setTimePeriods(CurrentLine, type, periods);
    if(m_calendar)
        m_calendar->setCurrentTimePeriods(type, periods);
    if(m_dayTimeWidget)
        m_dayTimeWidget->setPrimaryTimePeriods(type, periods);
}

void QnWorkbenchNavigator::updateCurrentBookmarks() {
    QnCameraBookmarkList bookmarks;
    if (QnCachingCameraDataLoader *loader = loaderByWidget(m_currentWidget))
        bookmarks = loader->bookmarks();
    m_timeSlider->setBookmarks(bookmarks);
}

void QnWorkbenchNavigator::updateSyncedPeriods() {
    for(int i = 0; i < Qn::TimePeriodContentCount; i++)
        updateSyncedPeriods(static_cast<Qn::TimePeriodContent>(i));
}

void QnWorkbenchNavigator::updateSyncedPeriods(Qn::TimePeriodContent type) {
    /* We don't want duplicate loaders. */
    QSet<QnCachingCameraDataLoader *> loaders;
    foreach(const QnResourceWidget *widget, m_syncedWidgets) {
        if(type == Qn::MotionContent && !(widget->options() & QnResourceWidget::DisplayMotion)) {
            /* Ignore it. */
        } else if(QnCachingCameraDataLoader *loader = loaderByWidget(widget)) {
            loaders.insert(loader);
        }
    }

    QVector<QnTimePeriodList> periodsList;
    foreach(QnCachingCameraDataLoader *loader, loaders)
        periodsList.push_back(loader->periods(type));

    QnTimePeriodList periods = QnTimePeriodList::mergeTimePeriods(periodsList);

    if (type == Qn::MotionContent) {
        foreach(QnMediaResourceWidget *widget, m_syncedWidgets) {
            QnAbstractArchiveReader  *archiveReader = widget->display()->archiveReader();
            if (archiveReader)
                archiveReader->setPlaybackMask(periods);
        }
    }

    m_timeSlider->setTimePeriods(SyncedLine, type, periods);
    if(m_calendar)
        m_calendar->setSyncedTimePeriods(type, periods);
    if(m_dayTimeWidget)
        m_dayTimeWidget->setSecondaryTimePeriods(type, periods);
}

void QnWorkbenchNavigator::updateLines() {
    bool isZoomed = display()->widget(Qn::ZoomedRole) != NULL;

    if(m_currentWidgetFlags & WidgetSupportsPeriods) {
        m_timeSlider->setLineVisible(CurrentLine, true);
        m_timeSlider->setLineVisible(SyncedLine, !isZoomed);

        m_timeSlider->setLineComment(CurrentLine, m_currentWidget->resource()->getName());
        m_timeSlider->setLineComment(SyncedLine, tr("All Cameras"));
    } else {
        m_timeSlider->setLineVisible(CurrentLine, false);
        m_timeSlider->setLineVisible(SyncedLine, false);
    }

    QnLayoutResourcePtr currentLayoutResource = workbench()->currentLayout()->resource().staticCast<QnLayoutResource>();
    if (context()->snapshotManager()->isFile(currentLayoutResource)) {
        m_timeSlider->setLastMinuteIndicatorVisible(CurrentLine, false);
        m_timeSlider->setLastMinuteIndicatorVisible(SyncedLine, false);
    } else {
        bool isSearch = workbench()->currentLayout()->data(Qn::LayoutSearchStateRole).value<QnThumbnailsSearchState>().step > 0;
        bool isLocal = m_currentWidget && m_currentWidget->resource()->flags().testFlag(Qn::local);

        bool hasNonLocalResource = !isLocal;
        if (!hasNonLocalResource) {
            foreach(const QnResourceWidget *widget, m_syncedWidgets) {
                if (widget->resource() && !widget->resource()->flags().testFlag(Qn::local)) {
                    hasNonLocalResource = true;
                    break;
                }
            }
        }

        m_timeSlider->setLastMinuteIndicatorVisible(CurrentLine, !isSearch && !isLocal);
        m_timeSlider->setLastMinuteIndicatorVisible(SyncedLine, !isSearch && hasNonLocalResource);
    }
}

void QnWorkbenchNavigator::updateCalendar() {
    if(!m_calendar)
        return;
    if(m_currentWidgetFlags & WidgetSupportsPeriods)
        m_calendar->setCurrentTimePeriodsVisible(true);
    else
        m_calendar->setCurrentTimePeriodsVisible(false);
}

void QnWorkbenchNavigator::updateSliderFromScrollBar() {
    if(m_updatingScrollBarFromSlider)
        return;

    QN_SCOPED_VALUE_ROLLBACK(&m_updatingSliderFromScrollBar, true);

    m_timeSlider->setWindow(m_timeScrollBar->value(), m_timeScrollBar->value() + m_timeScrollBar->pageStep());
}

void QnWorkbenchNavigator::updateScrollBarFromSlider() {
    if(m_updatingSliderFromScrollBar)
        return;

    {
        QN_SCOPED_VALUE_ROLLBACK(&m_updatingScrollBarFromSlider, true);

        qint64 windowSize = m_timeSlider->windowEnd() - m_timeSlider->windowStart();

        m_timeScrollBar->setRange(m_timeSlider->minimum(), m_timeSlider->maximum() - windowSize);
        m_timeScrollBar->setValue(m_timeSlider->windowStart());
        m_timeScrollBar->setPageStep(windowSize);
        m_timeScrollBar->setIndicatorPosition(m_timeSlider->sliderPosition());
    }

    updateSliderFromScrollBar(); /* Bi-directional sync is needed as time scrollbar may adjust the provided values. */
}

void QnWorkbenchNavigator::updateCalendarFromSlider(){
    m_calendar->setSelectedWindow(m_timeSlider->windowStart(), m_timeSlider->windowEnd());
    m_dayTimeWidget->setSelectedWindow(m_timeSlider->windowStart(), m_timeSlider->windowEnd());
}

void QnWorkbenchNavigator::updateLive() {
    bool live = isLive();
    if(m_lastLive == live)
        return;

    m_lastLive = live;
    
    if(live)
        setSpeed(1.0);

    emit liveChanged();
}

void QnWorkbenchNavigator::updateLiveSupported() {
    bool liveSupported = isLiveSupported();
    if(m_lastLiveSupported == liveSupported)
        return;

    m_lastLiveSupported = liveSupported;

    emit liveSupportedChanged();
}

void QnWorkbenchNavigator::updatePlaying() {
    bool playing = isPlaying();
    if(playing == m_lastPlaying)
        return;

    m_lastPlaying = playing;

    emit playingChanged();
}

void QnWorkbenchNavigator::updatePlayingSupported() {
    bool playingSupported = isPlayingSupported();
    if(playingSupported == m_lastPlayingSupported)
        return;

    m_lastPlayingSupported = playingSupported;

    emit playingSupportedChanged();
}

void QnWorkbenchNavigator::updateSpeed() {
    qreal speed = this->speed();
    if(qFuzzyCompare(m_lastSpeed, speed))
        return;

    m_lastSpeed = speed;

    emit speedChanged();
}

void QnWorkbenchNavigator::updateSpeedRange() {
    qreal minimalSpeed = this->minimalSpeed();
    qreal maximalSpeed = this->maximalSpeed();
    if(qFuzzyCompare(minimalSpeed, m_lastMinimalSpeed) && qFuzzyCompare(maximalSpeed, m_lastMaximalSpeed))
        return;

    m_lastMinimalSpeed = minimalSpeed;
    m_lastMaximalSpeed = maximalSpeed;
    
    emit speedRangeChanged();
}

void QnWorkbenchNavigator::updateThumbnailsLoader() {
    QnResourcePtr resource;
    qreal aspectRatio = -1;

    if (m_centralWidget) {
        aspectRatio = m_centralWidget->aspectRatio();
        aspectRatio /= QnGeometry::aspectRatio(m_centralWidget->channelLayout()->size());

        if(QnCachingCameraDataLoader *loader = loaderByWidget(m_centralWidget)) {
            if(!loader->periods(Qn::RecordingContent).isEmpty())
                resource = m_centralWidget->resource();
        } else if(m_currentMediaWidget && !m_currentMediaWidget->display()->isStillImage()) {
            resource = m_centralWidget->resource();
        }
    }

    QnMediaResourcePtr mediaResource = resource.dynamicCast<QnMediaResource>();
    bool isPanoramicCamera = mediaResource && mediaResource->getVideoLayout()->size().width() > 1;

    if(!resource || aspectRatio < 0 || isPanoramicCamera) {
        m_timeSlider->setThumbnailsLoader(NULL, -1);
    } else if(!m_timeSlider->thumbnailsLoader() || m_timeSlider->thumbnailsLoader()->resource() != resource) {
        m_timeSlider->setThumbnailsLoader(thumbnailLoader(resource), aspectRatio);
    }
}

void QnWorkbenchNavigator::updateTimeSliderWindowSizePolicy() {
    m_timeSlider->setOption(QnTimeSlider::PreserveWindowSize, m_timeSlider->isThumbnailsVisible());
}

void QnWorkbenchNavigator::setAutoPaused(bool autoPaused) {
    if (autoPaused == m_autoPaused)
        return;

    if (autoPaused) {
        /* Collect all playing resources */
        foreach (QnResourceWidget *widget, display()->widgets()) {
            QnMediaResourceWidget *mediaResourceWidget = dynamic_cast<QnMediaResourceWidget *>(widget);
            if (!mediaResourceWidget)
                continue;

            QnResourceDisplayPtr resourceDisplay = mediaResourceWidget->display();
            if (resourceDisplay->isPaused())
                continue;

            bool isLive = resourceDisplay->archiveReader() && resourceDisplay->archiveReader()->isRealTimeSource();
            resourceDisplay->pause();
            m_autoPausedResourceDisplays.insert(resourceDisplay, isLive);
        }
    } else if (m_autoPaused) {
        for (QHash<QnResourceDisplayPtr, bool>::iterator itr = m_autoPausedResourceDisplays.begin(); itr != m_autoPausedResourceDisplays.end(); ++itr) {
            itr.key()->play();
            if (itr.value())
                itr.key()->archiveReader()->jumpTo(DATETIME_NOW, 0);
        }

        m_autoPausedResourceDisplays.clear();
    }

    m_autoPaused = autoPaused;
    action(Qn::PlayPauseAction)->setEnabled(!m_autoPaused); /* Prevent special UI reaction on space key*/
}

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
bool QnWorkbenchNavigator::eventFilter(QObject *watched, QEvent *event) {
    if(watched == m_timeScrollBar && event->type() == QEvent::GraphicsSceneWheel) {
        if(m_timeSlider->scene() && m_timeSlider->scene()->sendEvent(m_timeSlider, event))
            return true;
    } else if(watched == m_timeScrollBar && event->type() == QEvent::GraphicsSceneMouseDoubleClick) {
        m_timeSlider->setWindow(m_timeSlider->minimum(), m_timeSlider->maximum(), true);
    }

    return base_type::eventFilter(watched, event);
}

void QnWorkbenchNavigator::at_timeSlider_customContextMenuRequested(const QPointF &pos, const QPoint &screenPos) {
    if(!context() || !context()->menu()) {
        qnWarning("Requesting context menu for a time slider while no menu manager instance is available.");
        return;
    }

    if (qnSettings->isVideoWallMode())
        return;

    QnActionManager *manager = context()->menu();

    QnTimePeriod selection;
    if(m_timeSlider->isSelectionValid())
        selection = QnTimePeriod(m_timeSlider->selectionStart(), m_timeSlider->selectionEnd() - m_timeSlider->selectionStart());

    qint64 position = m_timeSlider->valueFromPosition(pos);

    QnActionParameters parameters(currentTarget(Qn::SliderScope));
    parameters.setArgument(Qn::TimePeriodRole, selection);
    parameters.setArgument(Qn::TimePeriodsRole, m_timeSlider->timePeriods(CurrentLine, Qn::RecordingContent)); // TODO: #Elric move this out into global scope!
    parameters.setArgument(Qn::MergedTimePeriodsRole, m_timeSlider->timePeriods(SyncedLine, Qn::RecordingContent));
    if (m_currentWidget && m_timeSlider->timePeriods(CurrentLine, Qn::BookmarksContent).containTime(position)) {
        QnCameraBookmark bookmark;
        if (QnCachingCameraDataLoader *loader = loaderByWidget(m_currentMediaWidget))
            bookmark = loader->bookmarkByTime(position);
        if (!bookmark.isNull())
            parameters.setArgument(Qn::CameraBookmarkRole, bookmark);
    }
    

    QScopedPointer<QMenu> menu(manager->newMenu(
        Qn::SliderScope,
        mainWindow(),
        parameters
    ));
    if(menu->isEmpty())
        return;

    /* Add slider-local actions to the menu. */
    bool selectionEditable = m_timeSlider->options() & QnTimeSlider::SelectionEditable;
    manager->redirectAction(menu.data(), Qn::StartTimeSelectionAction,  selectionEditable ? m_startSelectionAction : NULL);
    manager->redirectAction(menu.data(), Qn::EndTimeSelectionAction,    selectionEditable ? m_endSelectionAction : NULL);
    manager->redirectAction(menu.data(), Qn::ClearTimeSelectionAction,  selectionEditable ? m_clearSelectionAction : NULL);

    /* Run menu. */
    QAction *action = menu->exec(screenPos);

    /* Process slider-local actions. */
    if(action == m_startSelectionAction) {
        m_timeSlider->setSelection(position, position);
        m_timeSlider->setSelectionValid(true);
    } else if(action == m_endSelectionAction) {
        m_timeSlider->setSelection(qMin(position, m_timeSlider->selectionStart()), qMax(position, m_timeSlider->selectionEnd()));
        m_timeSlider->setSelectionValid(true);
    } else if(action == m_clearSelectionAction) {
        m_timeSlider->setSelectionValid(false);
    } else if (action == context()->action(Qn::ZoomToTimeSelectionAction)) {
        if(!m_timeSlider->isSelectionValid())
            return;

        m_timeSlider->setValue((m_timeSlider->selectionStart() + m_timeSlider->selectionEnd()) / 2, false);
        m_timeSlider->finishAnimations();
        m_timeSlider->setWindow(m_timeSlider->selectionStart(), m_timeSlider->selectionEnd(), true);
    }
}

QnCachingCameraDataLoader* QnWorkbenchNavigator::loaderByWidget(const QnResourceWidget* widget) {
    if (!widget || !widget->resource())
        return NULL;
    return m_cameraDataManager->loader(widget->resource());
}

void QnWorkbenchNavigator::updateLoaderPeriods(const QnResourcePtr &resource, Qn::TimePeriodContent type) {
    if(m_currentWidget && m_currentWidget->resource() == resource)
        updateCurrentPeriods(type);
    
    if(m_syncedResources.contains(resource))
        updateSyncedPeriods(type);

    if(m_centralWidget && m_centralWidget->resource() == resource && type == Qn::RecordingContent)
        updateThumbnailsLoader();
}

void QnWorkbenchNavigator::updateLoaderBookmarks(const QnResourcePtr &resource) {
    if(!m_currentWidget || m_currentWidget->resource() != resource)
        return;

    QnCameraBookmarkList bookmarks = m_cameraDataManager->bookmarks(resource);
    m_timeSlider->setBookmarks(bookmarks);
}

void QnWorkbenchNavigator::at_timeSlider_valueChanged(qint64 value) {
    if(!m_currentWidget)
        return;

    if(isLive())
        value = DATETIME_NOW;

    /* Update tool tip format. */
    if (value == DATETIME_NOW) {
        //: Time slider's tooltip for position on live. 
        //: Note from QDateTime docs: any sequence of characters that are enclosed in single quotes will be treated as text and not be used as an expression for.
        //: That's where these single quotes come from.
        m_timeSlider->setToolTipFormat(tr("'Live'"));
    } else {
        if (m_currentWidgetFlags & WidgetUsesUTC) {
            //: This is a date/time format for time slider's tooltip. Please translate it only if you're absolutely sure that you know what you're doing.
            m_timeSlider->setToolTipFormat(tr("yyyy MMM dd\nhh:mm:ss"));
        } else {
            if(m_timeSlider->maximum() >= 60ll * 60ll * 1000ll) { /* Longer than 1 hour. */
                //: This is a date/time format for time slider's tooltip for local files. Please translate it only if you're absolutely sure that you know what you're doing.
                m_timeSlider->setToolTipFormat(tr("hh:mm:ss"));
            } else {
                //: This is a date/time format for time slider's tooltip for short local files. Please translate it only if you're absolutely sure that you know what you're doing.
                m_timeSlider->setToolTipFormat(tr("mm:ss"));
            }
        }
    }

    /* Update reader position. */
    if(m_currentMediaWidget && !m_updatingSliderFromReader) {
        if (QnAbstractArchiveReader *reader = m_currentMediaWidget->display()->archiveReader()){
            if (value == DATETIME_NOW) {
                reader->jumpTo(DATETIME_NOW, 0);
            } else {
                if (m_preciseNextSeek) {
                    reader->jumpTo(value * 1000, value * 1000); /* Precise seek. */
                }
                else if (m_timeSlider->isSliderDown()) {
                    reader->jumpTo(value * 1000, 0);
                }
                //else if (qnRedAssController->isPrecSeekAllowed(m_currentMediaWidget->display()->camDisplay()))
                //    reader->jumpTo(value * 1000, value * 1000); /* Precise seek. */
                else {
                    reader->setSkipFramesToTime(value * 1000); /* Precise seek. */
                }
                m_preciseNextSeek = false;
            }
        }

        updateLive();
        emit positionChanged();
    }
}

void QnWorkbenchNavigator::at_timeSlider_sliderPressed() {
    if (!m_currentWidget)
        return;

    if(!isPlaying())
        m_preciseNextSeek = true; /* When paused, use precise seeks on click. */

    if(m_lastPlaying) 
        setPlayingTemporary(false);

    m_pausedOverride = true;
}

void QnWorkbenchNavigator::at_timeSlider_sliderReleased() {
    if(!m_currentWidget)
        return;

    if(m_lastPlaying) 
        setPlayingTemporary(true);

    if(isPlaying()) {
        /* Handler must be re-run for precise seeking. */
        at_timeSlider_valueChanged(m_timeSlider->value());
        m_pausedOverride = false;
    }
}

void QnWorkbenchNavigator::at_timeSlider_selectionPressed() {
    if(!m_currentWidget)
        return;

    if(m_lastPlaying)
        setPlayingTemporary(true);
    setPlaying(false);

    m_pausedOverride = true;
}

void QnWorkbenchNavigator::at_timeSlider_selectionReleased() {
    if(!m_timeSlider->isSelectionValid())
        return;

    if(m_timeSlider->selectionStart() == m_timeSlider->selectionEnd())
        return;

    if(m_timeSlider->windowEnd() == m_timeSlider->maximum() && (m_currentWidgetFlags & WidgetSupportsLive))
        m_timeSlider->setWindowEnd(m_timeSlider->maximum() - 1); /* Go out of live. */
}

void QnWorkbenchNavigator::at_timeSlider_thumbnailClicked() {
    // use precise seek when positioning throuh thumbnail click
    m_preciseNextSeek = true;
}

void QnWorkbenchNavigator::at_display_widgetChanged(Qn::ItemRole role) {
    if(role == Qn::CentralRole)
        updateCentralWidget();

    if(role == Qn::ZoomedRole){
        updateLines();
        updateCalendar();
    }
}

void QnWorkbenchNavigator::at_display_widgetAdded(QnResourceWidget *widget) {
    if(widget->resource()->flags() & Qn::sync) {
        if(QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget)){
            addSyncedWidget(mediaWidget);
            connect(mediaWidget, SIGNAL(motionSelectionChanged()), this, SLOT(at_widget_motionSelectionChanged()));
        }
    }

    connect(widget, SIGNAL(aspectRatioChanged()), this, SLOT(updateThumbnailsLoader()));
    connect(widget, SIGNAL(optionsChanged()), this, SLOT(at_widget_optionsChanged()));
    connect(widget->resource(), SIGNAL(flagsChanged(const QnResourcePtr &)), this, SLOT(at_resource_flagsChanged(const QnResourcePtr &)));
}

void QnWorkbenchNavigator::at_display_widgetAboutToBeRemoved(QnResourceWidget *widget) {
    disconnect(widget, NULL, this, NULL);
    disconnect(widget->resource(), NULL, this, NULL);

    if(widget->resource()->flags() & Qn::sync)
        if(QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget))
            removeSyncedWidget(mediaWidget);
}

void QnWorkbenchNavigator::at_widget_motionSelectionChanged() {
    at_widget_motionSelectionChanged(checked_cast<QnMediaResourceWidget *>(sender()));
}

void QnWorkbenchNavigator::at_widget_motionSelectionChanged(QnMediaResourceWidget *widget) {
    /* We check that the loader can be created (i.e. that the resource is camera) 
     * just to feel safe. */
    if(QnCachingCameraDataLoader *loader = loaderByWidget(widget))
        loader->setMotionRegions(widget->motionSelection());
}

void QnWorkbenchNavigator::at_widget_optionsChanged() {
    at_widget_optionsChanged(checked_cast<QnResourceWidget *>(sender()));
}

void QnWorkbenchNavigator::at_widget_optionsChanged(QnResourceWidget *widget) {
    int oldSize = m_motionIgnoreWidgets.size();
    if(widget->options() & QnResourceWidget::DisplayMotion) {
        m_motionIgnoreWidgets.insert(widget);
    } else {
        m_motionIgnoreWidgets.remove(widget);
    }
    int newSize = m_motionIgnoreWidgets.size();

    if(oldSize != newSize) {
        updateSyncedPeriods(Qn::MotionContent);

        if(widget == m_currentWidget)
            updateCurrentPeriods(Qn::MotionContent);
    }
}

void QnWorkbenchNavigator::at_resource_flagsChanged(const QnResourcePtr &resource) {
    if(!resource || !m_currentWidget || m_currentWidget->resource() != resource)
        return;

    updateCurrentWidgetFlags();
}

void QnWorkbenchNavigator::at_timeScrollBar_sliderPressed() {
    m_timeSlider->setOption(QnTimeSlider::AdjustWindowToPosition, false);
}

void QnWorkbenchNavigator::at_timeScrollBar_sliderReleased() {
    m_timeSlider->setOption(QnTimeSlider::AdjustWindowToPosition, true);
}

void QnWorkbenchNavigator::at_calendar_dateClicked(const QDate &date){
    QDateTime dateTime(date);
    qint64 startMSec = dateTime.toMSecsSinceEpoch() - m_calendar->localOffset();
    qint64 endMSec = dateTime.addDays(1).toMSecsSinceEpoch() - m_calendar->localOffset();

    m_timeSlider->finishAnimations();
    if (QApplication::keyboardModifiers() == Qt::ControlModifier) {
        m_timeSlider->setWindow(
            qMin(startMSec, m_timeSlider->windowStart()),
            qMax(endMSec, m_timeSlider->windowEnd()),
            true
        );
    } else {
        m_timeSlider->setWindow(startMSec, endMSec, true);
    }
}

void QnWorkbenchNavigator::at_dayTimeWidget_timeClicked(const QTime &time) {
    QDate date = m_calendar->selectedDate();
    if(!date.isValid())
        return;

    QDateTime dateTime = QDateTime(date, time);

    qint64 startMSec = dateTime.toMSecsSinceEpoch() - m_dayTimeWidget->localOffset();
    qint64 endMSec = dateTime.addSecs(60 * 60).toMSecsSinceEpoch() - m_dayTimeWidget->localOffset();

    m_timeSlider->finishAnimations();
    if (QApplication::keyboardModifiers() == Qt::ControlModifier) {
        m_timeSlider->setWindow(
            qMin(startMSec, m_timeSlider->windowStart()),
            qMax(endMSec, m_timeSlider->windowEnd()),
            true
        );
    } else {
        m_timeSlider->setWindow(startMSec, endMSec, true);
    }
}

QnSearchLineEdit * QnWorkbenchNavigator::bookmarksSearchWidget() const {
    return m_bookmarksSearchWidget;
}

void QnWorkbenchNavigator::setBookmarksSearchWidget(QnSearchLineEdit *bookmarksSearchWidget) {
    if(m_bookmarksSearchWidget == bookmarksSearchWidget)
        return;

    if(m_bookmarksSearchWidget) {
        disconnect(m_bookmarksSearchWidget, NULL, this, NULL);

        if(isValid())
            deinitialize();
    }

    m_bookmarksSearchWidget = bookmarksSearchWidget;

    if(m_bookmarksSearchWidget) {
        connect(m_bookmarksSearchWidget, &QObject::destroyed, this, [this](){setBookmarksSearchWidget(NULL);});

        if(isValid())
            initialize();
    }
}

QnCameraBookmarkTags QnWorkbenchNavigator::bookmarkTags() const {
    return m_bookmarkTags;
}

void QnWorkbenchNavigator::setBookmarkTags(const QnCameraBookmarkTags &tags) {
    if (m_bookmarkTags == tags)
        return;
    m_bookmarkTags = tags;

    if (!isValid())
        return;

    QCompleter *completer = new QCompleter(m_bookmarkTags);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setCompletionMode(QCompleter::InlineCompletion);

    m_bookmarksSearchWidget->lineEdit()->setCompleter(completer);
    m_bookmarkTagsCompleter.reset(completer);
}
