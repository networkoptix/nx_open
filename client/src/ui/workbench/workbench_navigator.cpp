#include "workbench_navigator.h"

#include <cassert>

#include <QtCore/QTimer>

#include <QtGui/QAction>
#include <QtGui/QMenu>
#include <QtGui/QGraphicsSceneContextMenuEvent>
#include <QtGui/QApplication>

#include <utils/common/util.h>
#include <utils/common/synctime.h>
#include <utils/common/scoped_value_rollback.h>
#include <utils/common/checked_cast.h>

#include <core/resource/camera_resource.h>

#include <camera/caching_time_period_loader.h>
#include <camera/time_period_loader.h>
#include <camera/resource_display.h>
#include <camera/video_camera.h>

#include <ui/actions/action_manager.h>
#include <ui/actions/action_parameter_types.h>
#include <ui/graphics/items/resource/resource_widget.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/items/controls/time_slider.h>
#include <ui/graphics/items/controls/time_scroll_bar.h>
#include <ui/graphics/instruments/signaling_instrument.h>
#include <ui/widgets/calendar_widget.h>

#include "extensions/workbench_stream_synchronizer.h"
#include "workbench.h"
#include "workbench_display.h"
#include "workbench_context.h"
#include "workbench_item.h"
#include "workbench_layout.h"
#include "workbench_layout_snapshot_manager.h"

#include "camera/thumbnails_loader.h"
#include "plugins/resources/archive/abstract_archive_stream_reader.h"
#include "libavutil/avutil.h" // TODO: remove
#include "handlers/workbench_action_handler.h"

QnWorkbenchNavigator::QnWorkbenchNavigator(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_streamSynchronizer(context()->instance<QnWorkbenchStreamSynchronizer>()),
    m_timeSlider(NULL),
    m_timeScrollBar(NULL),
    m_calendar(NULL),
    m_centralWidget(NULL),
    m_currentWidget(NULL),
    m_currentMediaWidget(NULL),
    m_currentWidgetFlags(0),
    m_currentWidgetLoaded(false),
    m_currentWidgetIsCentral(false),
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
    m_lastSpeed(0.0),
    m_lastMinimalSpeed(0.0),
    m_lastMaximalSpeed(0.0),
    m_lastUpdateSlider(0),
    m_lastCameraTime(0),
    m_startSelectionAction(new QAction(this)),
    m_endSelectionAction(new QAction(this)),
    m_clearSelectionAction(new QAction(this))
{}
    
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
        connect(m_timeSlider, SIGNAL(destroyed()), this, SLOT(at_timeSlider_destroyed()));

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
        connect(m_timeScrollBar, SIGNAL(destroyed()), this, SLOT(at_timeScrollBar_destroyed()));

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
        connect(m_calendar, SIGNAL(destroyed()), this, SLOT(at_calendar_destroyed()));

        if(isValid())
            initialize();
    }
}

bool QnWorkbenchNavigator::isValid() {
    return m_timeSlider && m_timeScrollBar && m_calendar;
}

void QnWorkbenchNavigator::initialize() {
    assert(isValid());

    connect(workbench(),                        SIGNAL(currentLayoutChanged()),                     this,   SLOT(updateSliderOptions()));

    connect(display(),                          SIGNAL(widgetChanged(Qn::ItemRole)),                this,   SLOT(at_display_widgetChanged(Qn::ItemRole)));
    connect(display(),                          SIGNAL(widgetAdded(QnResourceWidget *)),            this,   SLOT(at_display_widgetAdded(QnResourceWidget *)));
    connect(display(),                          SIGNAL(widgetAboutToBeRemoved(QnResourceWidget *)), this,   SLOT(at_display_widgetAboutToBeRemoved(QnResourceWidget *)));
    connect(display()->beforePaintInstrument(), SIGNAL(activated(QWidget *, QEvent *)),             this,   SLOT(updateSliderFromReader()));

    connect(m_streamSynchronizer,               SIGNAL(runningChanged()),                           this,   SLOT(updateCurrentWidget()));

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

    connect(m_calendar,                         SIGNAL(dateClicked(const QDate&)),                  this,   SLOT(at_calendar_dateChanged(const QDate&)));
    connect(m_calendar,                         SIGNAL(currentPageChanged(int,int)),                this,   SLOT(updateTargetPeriod()));

    updateLines();
    updateCalendar();
    updateScrollBarFromSlider();
    updateTimeSliderWindowSizePolicy();
} 

void QnWorkbenchNavigator::deinitialize() {
    assert(isValid());

    disconnect(action(Qn::SelectionChangeAction),   NULL, this, NULL);

    disconnect(display(),                           NULL, this, NULL);
    disconnect(display()->beforePaintInstrument(),  NULL, this, NULL);
    
    disconnect(m_timeSlider,                        NULL, this, NULL);
    m_timeSlider->removeEventFilter(this);

    disconnect(m_timeScrollBar,                     NULL, this, NULL);
    m_timeScrollBar->removeEventFilter(this);

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

    if (live){
        m_timeSlider->setValue(m_timeSlider->maximum(), true);
    } else {
        m_timeSlider->setValue(m_timeSlider->minimum(), true); // TODO: need to save position here.
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

    m_pausedOverride = false;

    QnAbstractArchiveReader *reader = m_currentMediaWidget->display()->archiveReader();
    QnCamDisplay *camDisplay = m_currentMediaWidget->display()->camDisplay();
    if(playing) {
        if (reader->isRealTimeSource()) {
            /* Media was paused while on live. Jump to archive when resumed. */
            qint64 time = camDisplay->getCurrentTime();
            reader->resumeMedia();
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

void QnWorkbenchNavigator::addSyncedWidget(QnMediaResourceWidget *widget) {
    if (widget == NULL) {
        qnNullWarning(widget);
        return;
    }

    m_syncedWidgets.insert(widget);
    m_syncedResources.insert(widget->resource(), QHashDummyValue());

    updateCurrentWidget();
    updateSyncedPeriods();
}

void QnWorkbenchNavigator::removeSyncedWidget(QnMediaResourceWidget *widget) {
    if(!m_syncedWidgets.remove(widget))
        return;

    if(m_syncedWidgets.contains(m_currentMediaWidget))
        updateItemDataFromSlider(widget);

    /* QHash::erase does nothing when called for container's end, 
     * and is therefore perfectly safe. */
    m_syncedResources.erase(m_syncedResources.find(widget->resource()));
    m_motionIgnoreWidgets.remove(widget);

    if(QnCachingTimePeriodLoader *loader = this->loader(widget->resource()))
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
    if(m_timeSlider->windowEnd() == m_timeSlider->maximum()) // TODO: check that widget supports live.
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

QnCachingTimePeriodLoader *QnWorkbenchNavigator::loader(const QnResourcePtr &resource) {
    QHash<QnResourcePtr, QnCachingTimePeriodLoader *>::const_iterator pos = m_loaderByResource.find(resource);
    if(pos != m_loaderByResource.end())
        return *pos;

    QnCachingTimePeriodLoader *loader = QnCachingTimePeriodLoader::newInstance(resource, this);
    if(loader)
        connect(loader, SIGNAL(periodsChanged(Qn::TimePeriodRole)), this, SLOT(at_loader_periodsChanged(Qn::TimePeriodRole)));

    m_loaderByResource[resource] = loader;
    return loader;
}

QnCachingTimePeriodLoader *QnWorkbenchNavigator::loader(QnResourceWidget *widget) {
    return widget ? loader(widget->resource()) : NULL;
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
    if(QnCachingTimePeriodLoader *loader = this->loader(m_currentMediaWidget)) {
        QnTimePeriodList periods = loader->periods(loader->isMotionRegionsEmpty() ? Qn::RecordingRole : Qn::MotionRole);
        if (loader->isMotionRegionsEmpty())
            periods = QnTimePeriod::aggregateTimePeriods(periods, MAX_FRAME_DURATION);
        
        if (!periods.isEmpty()) {
            qint64 currentTime = m_currentMediaWidget->display()->camera()->getCurrentTime();

            if (currentTime == DATETIME_NOW) {
                pos = periods.last().startTimeMs * 1000;
            } else {
                QnTimePeriodList::const_iterator itr = qUpperBound(periods.begin(), periods.end(), currentTime / 1000);
                itr = qMax(itr - 2, periods.constBegin());
                pos = itr->startTimeMs * 1000;
                if (reader->isReverseMode() && itr->durationMs != -1)
                    pos += itr->durationMs * 1000;
            }
        }
    }
    reader->jumpTo(pos, 0);
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
    } else {
        QnCachingTimePeriodLoader *loader = this->loader(m_currentMediaWidget);
        QnTimePeriodList periods = loader->periods(loader->isMotionRegionsEmpty() ? Qn::RecordingRole : Qn::MotionRole);
        if (loader->isMotionRegionsEmpty())
            periods = QnTimePeriod::aggregateTimePeriods(periods, MAX_FRAME_DURATION);

        QnTimePeriodList::const_iterator itr = qUpperBound(periods.begin(), periods.end(), m_currentMediaWidget->display()->camera()->getCurrentTime() / 1000);
        if (itr == periods.end() || (reader->isReverseMode() && itr->durationMs == -1)) {
            pos = DATETIME_NOW;
        } else {
            pos = (itr->startTimeMs + (reader->isReverseMode() ? itr->durationMs : 0)) * 1000;
        }
    }
    reader->jumpTo(pos, 0);
}

void QnWorkbenchNavigator::stepBackward() {
    if(!m_currentMediaWidget)
        return;

    QnAbstractArchiveReader *reader = m_currentMediaWidget->display()->archiveReader();
    if(!reader)
        return;

    m_pausedOverride = false;

    if (!reader->isSkippingFrames() && reader->currentTime() > reader->startTime()) {
        quint64 currentTime = m_currentMediaWidget->display()->camera()->getCurrentTime();

        if (reader->isSingleShotMode())
            m_currentMediaWidget->display()->camDisplay()->playAudio(false); // TODO: wtf?

        reader->previousFrame(currentTime);
    }
}

void QnWorkbenchNavigator::stepForward() {
    if(!m_currentMediaWidget)
        return;

    QnAbstractArchiveReader *reader = m_currentMediaWidget->display()->archiveReader();
    if(!reader)
        return;

    m_pausedOverride = false;

    reader->nextFrame();
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
    QnResourceWidget *widget = NULL;
    bool isCentral = false;
    if (m_centralWidget != NULL || !m_streamSynchronizer->isRunning()) {
        widget = m_centralWidget;
        isCentral = true;
    } else if(m_syncedWidgets.contains(m_currentMediaWidget)) {
        widget = m_currentMediaWidget;
    } else if (m_syncedWidgets.empty()) {
        widget = NULL;
    } else {
        widget = *m_syncedWidgets.begin();
    }
    QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget);

    if (m_currentWidget == widget) {
        if(m_currentWidgetIsCentral != isCentral) {
            m_currentWidgetIsCentral = isCentral;
            updateLines();
            updateCalendar();
        }

        return;
    }

    emit currentWidgetAboutToBeChanged();

    WidgetFlags previousWidgetFlags = m_currentWidgetFlags;

    if(m_currentWidget) {
        updateItemDataFromSlider(m_currentWidget);
    } else {
        m_sliderDataInvalid = true;
        m_sliderWindowInvalid = true;
    }

    if(m_currentMediaWidget) {
        QnAbstractArchiveReader *archiveReader = m_currentMediaWidget->display()->archiveReader();
        if (archiveReader)
            archiveReader->setPlaybackMask(QnTimePeriodList());
    }

    m_currentWidget = widget;
    m_currentMediaWidget = mediaWidget;

    m_pausedOverride = false;
    m_currentWidgetLoaded = false;
    m_currentWidgetIsCentral = isCentral;

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
        QMetaObject::invokeMethod(this, "updatePlaying", Qt::QueuedConnection); // TODO: evil hacks...
        QMetaObject::invokeMethod(this, "updateSpeed", Qt::QueuedConnection);
    }

    updateCurrentPeriods();
    updateLiveSupported();
    updateLive();
    updatePlayingSupported();
    updatePlaying();
    updateSpeedRange();
    updateSpeed();

    emit currentWidgetChanged();
}

void QnWorkbenchNavigator::updateCurrentWidgetFlags() {
    WidgetFlags flags = 0;

    if(m_currentWidget) {
        flags = 0;

        if(m_currentWidget->resource().dynamicCast<QnSecurityCamResource>())
            flags |= WidgetSupportsLive | WidgetSupportsPeriods;

        if(m_currentWidget->resource()->flags() & QnResource::periods)
            flags |= WidgetSupportsPeriods;

        if(m_currentWidget->resource()->flags() & QnResource::utc)
            flags |= WidgetUsesUTC;

        if(m_currentWidget->resource()->flags() & QnResource::sync)
            flags |= WidgetSupportsSync;

        QnThumbnailsSearchState searchState = workbench()->currentLayout()->data(Qn::LayoutSearchStateRole).value<QnThumbnailsSearchState>();
        if(searchState.step > 0) /* Is a thumbnails search layout. */
            flags &= ~(WidgetSupportsLive | WidgetSupportsSync);
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

    bool selectionEditable = workbench()->currentLayout()->resource(); //&& (workbench()->currentLayout()->resource()->flags() & QnResource::local_media) != QnResource::local_media;
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

    QnScopedValueRollback<bool> guard(&m_updatingSliderFromReader, true);

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

        startTimeUSec = reader->startTime();                       /* vvvvv  If nothing is recorded, set minimum to end - 10s. */
        startTimeMSec = startTimeUSec == DATETIME_NOW ? endTimeMSec - 10000 : ((quint64)startTimeUSec == AV_NOPTS_VALUE ? m_timeSlider->minimum() : startTimeUSec / 1000);
    }

    m_timeSlider->setRange(startTimeMSec, endTimeMSec);
    if(m_calendar)
        m_calendar->setDateRange(QDateTime::fromMSecsSinceEpoch(startTimeMSec).date(), QDateTime::fromMSecsSinceEpoch(endTimeMSec).date());

    if(!m_pausedOverride) {
        qint64 timeUSec = m_currentMediaWidget->display()->camDisplay()->isRealTimeSource() ? DATETIME_NOW : m_currentMediaWidget->display()->camera()->getCurrentTime();
        if(isSearch && (quint64)timeUSec == AV_NOPTS_VALUE) {
            timeUSec = m_currentMediaWidget->item()->data<qint64>(Qn::ItemTimeRole, AV_NOPTS_VALUE);
            if((quint64)timeUSec != AV_NOPTS_VALUE)
                timeUSec *= 1000;
        }
        qint64 timeMSec = timeUSec == DATETIME_NOW ? endTimeMSec : ((quint64)timeUSec == AV_NOPTS_VALUE ? m_timeSlider->value() : timeUSec / 1000);
        qint64 timeNext = m_currentMediaWidget->display()->camDisplay()->isRealTimeSource() ? AV_NOPTS_VALUE : m_currentMediaWidget->display()->camDisplay()->getNextTime();

        if (timeUSec != DATETIME_NOW && (quint64)timeUSec != AV_NOPTS_VALUE){
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

        if((quint64)timeUSec != AV_NOPTS_VALUE)
            updateLive();

        if(isSearch) {
            QVector<qint64> indicators;
            foreach(QnResourceWidget *widget, display()->widgets())
                if(QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget))
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

    /* Update target time period for time period loaders. 
     * If playback is synchronized, do it for all cameras. */
    QnTimePeriod targetPeriod(m_timeSlider->windowStart(), m_timeSlider->windowEnd() - m_timeSlider->windowStart());
    QnTimePeriod boundingPeriod(m_timeSlider->minimum(), m_timeSlider->maximum() - m_timeSlider->minimum());

    if (m_calendar) {
        QDate date(m_calendar->yearShown(), m_calendar->monthShown(), 1);
        QnTimePeriod calendarPeriod(QDateTime(date).toMSecsSinceEpoch(), QDateTime(date.addMonths(1)).toMSecsSinceEpoch() - QDateTime(date).toMSecsSinceEpoch());
        // todo: signal deadlock here!
        // #GDM, fix it!
        //period.addPeriod(calendarPeriod);
    }

    if(m_streamSynchronizer->isRunning() && (m_currentWidgetFlags & WidgetSupportsPeriods)) {
        foreach(QnResourceWidget *widget, m_syncedWidgets)
            if(QnCachingTimePeriodLoader *loader = this->loader(widget))
                loader->setTargetPeriods(targetPeriod, boundingPeriod);
    } else if(m_currentWidgetFlags & WidgetSupportsPeriods) {
        if(QnCachingTimePeriodLoader *loader = this->loader(m_currentWidget))
            loader->setTargetPeriods(targetPeriod, boundingPeriod);
    }
}

void QnWorkbenchNavigator::updateCurrentPeriods() {
    for(int i = 0; i < Qn::TimePeriodRoleCount; i++)
        updateCurrentPeriods(static_cast<Qn::TimePeriodRole>(i));
}

void QnWorkbenchNavigator::updateCurrentPeriods(Qn::TimePeriodRole type) {
    QnTimePeriodList periods;

    if(type == Qn::MotionRole && m_currentWidget && !(m_currentWidget->options() & QnResourceWidget::DisplayMotion)) {
        /* Use empty periods. */
    } else if(QnCachingTimePeriodLoader *loader = this->loader(m_currentWidget)) {
        periods = loader->periods(type);
    }

    m_timeSlider->setTimePeriods(CurrentLine, type, periods);
    if(m_calendar)
        m_calendar->setCurrentTimePeriods(type, periods);
}

void QnWorkbenchNavigator::updateSyncedPeriods() {
    for(int i = 0; i < Qn::TimePeriodRoleCount; i++)
        updateSyncedPeriods(static_cast<Qn::TimePeriodRole>(i));
}

void QnWorkbenchNavigator::updateSyncedPeriods(Qn::TimePeriodRole type) {
    QVector<QnTimePeriodList> periodsList;
    foreach(const QnResourceWidget *widget, m_syncedWidgets) {
        if(type == Qn::MotionRole && !(widget->options() & QnResourceWidget::DisplayMotion)) {
            /* Ignore it. */
        } else if(QnCachingTimePeriodLoader *loader = this->loader(widget->resource())) {
            periodsList.push_back(loader->periods(type));
        }
    }

    QnTimePeriodList periods = QnTimePeriod::mergeTimePeriods(periodsList);

    if (type == Qn::MotionRole) {
        foreach(QnMediaResourceWidget *widget, m_syncedWidgets) {
            QnAbstractArchiveReader  *archiveReader = widget->display()->archiveReader();
            if (archiveReader)
                archiveReader->setPlaybackMask(periods);
        }
    }

    m_timeSlider->setTimePeriods(SyncedLine, type, periods);
    if(m_calendar)
        m_calendar->setSyncedTimePeriods(type, periods);
}

void QnWorkbenchNavigator::updateLines() {
    bool isZoomed = display()->widget(Qn::ZoomedRole) != NULL;

    if(m_currentWidgetFlags & WidgetSupportsPeriods) {
        m_timeSlider->setLineVisible(CurrentLine, m_currentWidgetIsCentral);
        m_timeSlider->setLineVisible(SyncedLine, !isZoomed);

        m_timeSlider->setLineComment(CurrentLine, m_currentWidget->resource()->getName());
        m_timeSlider->setLineComment(SyncedLine, tr("All Cameras"));
    } else {
        m_timeSlider->setLineVisible(CurrentLine, false);
        m_timeSlider->setLineVisible(SyncedLine, false);
    }
}

void QnWorkbenchNavigator::updateCalendar(){
    if (!m_calendar)
        return;
    if(m_currentWidgetFlags & WidgetSupportsPeriods)
        m_calendar->setCurrentWidgetIsCentral(m_currentWidgetIsCentral);
    else
        m_calendar->setCurrentWidgetIsCentral(false);
}

void QnWorkbenchNavigator::updateSliderFromScrollBar() {
    if(m_updatingScrollBarFromSlider)
        return;

    QnScopedValueRollback<bool> guard(&m_updatingSliderFromScrollBar, true);

    m_timeSlider->setWindow(m_timeScrollBar->value(), m_timeScrollBar->value() + m_timeScrollBar->pageStep());
}

void QnWorkbenchNavigator::updateScrollBarFromSlider() {
    if(m_updatingSliderFromScrollBar)
        return;

    {
        QnScopedValueRollback<bool> guard(&m_updatingScrollBarFromSlider, true);

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

    if (m_centralWidget) {
        if(QnCachingTimePeriodLoader *loader = this->loader(m_centralWidget)) {
            if(!loader->periods(Qn::RecordingRole).isEmpty())
                resource = m_centralWidget->resource();
        } else if(m_currentMediaWidget && !m_currentMediaWidget->display()->isStillImage()) {
            resource = m_centralWidget->resource();
        }
    }

    if(!resource) {
        m_timeSlider->setThumbnailsLoader(NULL);
    } else if(!m_timeSlider->thumbnailsLoader() || m_timeSlider->thumbnailsLoader()->resource() != resource) {
        m_timeSlider->setThumbnailsLoader(thumbnailLoader(resource));
    }
}

void QnWorkbenchNavigator::updateTimeSliderWindowSizePolicy() {
    m_timeSlider->setOption(QnTimeSlider::PreserveWindowSize, m_timeSlider->isThumbnailsVisible());
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
    QnActionManager *manager = context()->menu();

    QnTimePeriod selection;
    if(m_timeSlider->isSelectionValid())
        selection = QnTimePeriod(m_timeSlider->selectionStart(), m_timeSlider->selectionEnd() - m_timeSlider->selectionStart());

    QScopedPointer<QMenu> menu(manager->newMenu(
        Qn::SliderScope, 
        QnActionParameters(currentTarget(Qn::SliderScope)).
            withArgument(Qn::TimePeriodParameter, selection).
            withArgument(Qn::TimePeriodsParameter, m_timeSlider->timePeriods(CurrentLine, Qn::RecordingRole)) // TODO: move this out into global scope!
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
        qint64 position = m_timeSlider->valueFromPosition(pos);
        m_timeSlider->setSelection(position, position);
        m_timeSlider->setSelectionValid(true);
    } else if(action == m_endSelectionAction) {
        qint64 position = m_timeSlider->valueFromPosition(pos);
        m_timeSlider->setSelection(qMin(position, m_timeSlider->selectionStart()), qMax(position, m_timeSlider->selectionEnd()));
        m_timeSlider->setSelectionValid(true);
    } else if(action == m_clearSelectionAction) {
        m_timeSlider->setSelectionValid(false);
    }
}

void QnWorkbenchNavigator::at_loader_periodsChanged(Qn::TimePeriodRole type) {
    at_loader_periodsChanged(checked_cast<QnCachingTimePeriodLoader *>(sender()), type);
}

void QnWorkbenchNavigator::at_loader_periodsChanged(QnCachingTimePeriodLoader *loader, Qn::TimePeriodRole type) {
    QnResourcePtr resource = loader->resource();

    if(m_currentWidget && m_currentWidget->resource() == resource)
        updateCurrentPeriods(type);
    
    if(m_syncedResources.contains(resource))
        updateSyncedPeriods(type);

    if(m_centralWidget && m_centralWidget->resource() == resource)
        updateThumbnailsLoader();
}

void QnWorkbenchNavigator::at_timeSlider_valueChanged(qint64 value) {
    if(!m_currentWidget)
        return;

    if(isLive())
        value = DATETIME_NOW;

    /* Update tool tip format. */
    if (value == DATETIME_NOW) {
        m_timeSlider->setToolTipFormat(tr("'Live'", "LIVE_TOOL_TIP_FORMAT"));
    } else {
        if (m_currentWidgetFlags & WidgetUsesUTC) {
            m_timeSlider->setToolTipFormat(tr("yyyy MMM dd\nhh:mm:ss", "CAMERA_TOOL_TIP_FORMAT"));
        } else {
            if(m_timeSlider->maximum() >= 60ll * 60ll * 1000ll) { /* Longer than 1 hour. */
                m_timeSlider->setToolTipFormat(tr("hh:mm:ss", "LONG_TOOL_TIP_FORMAT"));
            } else {
                m_timeSlider->setToolTipFormat(tr("mm:ss", "SHORT_TOOL_TIP_FORMAT"));
            }
        }
    }

    /* Update reader position. */
    if(m_currentMediaWidget && !m_updatingSliderFromReader) {
        if (QnAbstractArchiveReader *reader = m_currentMediaWidget->display()->archiveReader()){
            if (value == DATETIME_NOW) {
                reader->jumpToPreviousFrame(DATETIME_NOW);
            } else {
                if (m_timeSlider->isSliderDown() && !m_preciseNextSeek) {
                    reader->jumpTo(value * 1000, 0);
                } else {
                    reader->jumpTo(value * 1000, value * 1000); /* Precise seek. */
                    m_preciseNextSeek = false;
                }
            }
        }

        updateLive();
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

    if(isPlaying())
        m_pausedOverride = false;

    /* Handler must be re-run for precise seeking. */
    at_timeSlider_valueChanged(m_timeSlider->value());
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

void QnWorkbenchNavigator::at_display_widgetChanged(Qn::ItemRole role) {
    if(role == Qn::CentralRole)
        updateCentralWidget();

    if(role == Qn::ZoomedRole){
        updateLines();
        updateCalendar();
    }
}

void QnWorkbenchNavigator::at_display_widgetAdded(QnResourceWidget *widget) {
    if(widget->resource()->flags() & QnResource::sync)
        if(QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget)){
            addSyncedWidget(mediaWidget);
            connect(mediaWidget, SIGNAL(motionSelectionChanged()), this, SLOT(at_widget_motionSelectionChanged()));
        }

    connect(widget, SIGNAL(optionsChanged()), this, SLOT(at_widget_optionsChanged()));
    connect(widget->resource().data(), SIGNAL(flagsChanged()), this, SLOT(at_resource_flagsChanged()));
}

void QnWorkbenchNavigator::at_display_widgetAboutToBeRemoved(QnResourceWidget *widget) {
    disconnect(widget, NULL, this, NULL);
    disconnect(widget->resource().data(), NULL, this, NULL);

    if(widget->resource()->flags() & QnResource::sync)
        if(QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget))
            removeSyncedWidget(mediaWidget);
}

void QnWorkbenchNavigator::at_widget_motionSelectionChanged() {
    at_widget_motionSelectionChanged(checked_cast<QnMediaResourceWidget *>(sender()));
}

void QnWorkbenchNavigator::at_widget_motionSelectionChanged(QnMediaResourceWidget *widget) {
    /* We check that the loader can be created (i.e. that the resource is camera) 
     * just to feel safe. */
    if(QnCachingTimePeriodLoader *loader = this->loader(widget->resource()))
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
        updateSyncedPeriods(Qn::MotionRole);

        if(widget == m_currentWidget)
            updateCurrentPeriods(Qn::MotionRole);
    }
}

void QnWorkbenchNavigator::at_resource_flagsChanged() {
    if(!sender())
        return;

    at_resource_flagsChanged(checked_cast<QnResource *>(sender())->toSharedPointer());
}

void QnWorkbenchNavigator::at_resource_flagsChanged(const QnResourcePtr &resource) {
    if(!resource || !m_currentWidget || m_currentWidget->resource() != resource)
        return;

    updateCurrentWidgetFlags();
}

void QnWorkbenchNavigator::at_timeSlider_destroyed() {
    setTimeSlider(NULL);
}

void QnWorkbenchNavigator::at_timeScrollBar_destroyed() {
    setTimeScrollBar(NULL);
}

void QnWorkbenchNavigator::at_calendar_destroyed() {
    setCalendar(NULL);
}

void QnWorkbenchNavigator::at_timeScrollBar_sliderPressed() {
    m_timeSlider->setOption(QnTimeSlider::AdjustWindowToPosition, false);
}

void QnWorkbenchNavigator::at_timeScrollBar_sliderReleased() {
    m_timeSlider->setOption(QnTimeSlider::AdjustWindowToPosition, true);
}

void QnWorkbenchNavigator::at_calendar_dateChanged(const QDate &date){
    QDateTime dt(date);
    qint64 startMSec = dt.toMSecsSinceEpoch();
    qint64 endMSec = dt.addDays(1).toMSecsSinceEpoch();
    m_timeSlider->finishAnimations();
    m_timeSlider->setWindow(startMSec, endMSec, true);
}

