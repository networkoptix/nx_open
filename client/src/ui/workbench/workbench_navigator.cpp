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
#include <camera/camera.h>

#include <ui/actions/action_manager.h>
#include <ui/actions/action_parameter_types.h>
#include <ui/graphics/items/resource_widget.h>
#include <ui/graphics/items/controls/time_slider.h>
#include <ui/graphics/items/controls/time_scroll_bar.h>
#include <ui/graphics/instruments/signaling_instrument.h>

#include "workbench.h"
#include "workbench_display.h"
#include "workbench_context.h"
#include "workbench_item.h"
#include "workbench_layout.h"
#include "workbench_layout_snapshot_manager.h"

#include "camera/thumbnails_loader.h"
#include "plugins/resources/archive/abstract_archive_stream_reader.h"
#include "libavutil/avutil.h" // TODO: remove

QnWorkbenchNavigator::QnWorkbenchNavigator(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_timeSlider(NULL),
    m_timeScrollBar(NULL),
    m_centralWidget(NULL),
    m_currentWidget(NULL),
    m_updatingSliderFromReader(false),
    m_updatingSliderFromScrollBar(false),
    m_updatingScrollBarFromSlider(false),
    m_currentWidgetFlags(0),
    m_currentWidgetLoaded(false),
    m_currentWidgetIsCentral(false),
    m_startSelectionAction(new QAction(this)),
    m_endSelectionAction(new QAction(this)),
    m_clearSelectionAction(new QAction(this)),
    m_lastLive(false),
    m_lastLiveSupported(false),
    m_lastPlaying(false),
    m_lastPlayingSupported(false),
    m_pausedOverride(false),
    m_lastSpeed(0.0),
    m_lastMinimalSpeed(0.0),
    m_lastMaximalSpeed(0.0)
{}
    
QnWorkbenchNavigator::~QnWorkbenchNavigator() {
    return;
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

bool QnWorkbenchNavigator::isValid() {
    return m_timeSlider && m_timeScrollBar;
}

void QnWorkbenchNavigator::initialize() {
    assert(isValid());

    connect(workbench(),                        SIGNAL(currentLayoutChanged()),                     this,   SLOT(updateSliderOptions()));

    connect(display(),                          SIGNAL(widgetChanged(Qn::ItemRole)),                this,   SLOT(at_display_widgetChanged(Qn::ItemRole)));
    connect(display(),                          SIGNAL(widgetAdded(QnResourceWidget *)),            this,   SLOT(at_display_widgetAdded(QnResourceWidget *)));
    connect(display(),                          SIGNAL(widgetAboutToBeRemoved(QnResourceWidget *)), this,   SLOT(at_display_widgetAboutToBeRemoved(QnResourceWidget *)));
    connect(display(),                          SIGNAL(streamsSynchronizedChanged()),               this,   SLOT(updateCurrentWidget()));
    connect(display()->beforePaintInstrument(), SIGNAL(activated(QWidget *, QEvent *)),             this,   SLOT(updateSliderFromReader()));
    
    connect(m_timeSlider,                       SIGNAL(valueChanged(qint64)),                       this,   SLOT(at_timeSlider_valueChanged(qint64)));
    connect(m_timeSlider,                       SIGNAL(sliderPressed()),                            this,   SLOT(at_timeSlider_sliderPressed()));
    connect(m_timeSlider,                       SIGNAL(sliderReleased()),                           this,   SLOT(at_timeSlider_sliderReleased()));
    connect(m_timeSlider,                       SIGNAL(valueChanged(qint64)),                       this,   SLOT(updateScrollBarFromSlider()));
    connect(m_timeSlider,                       SIGNAL(rangeChanged(qint64, qint64)),               this,   SLOT(updateScrollBarFromSlider()));
    connect(m_timeSlider,                       SIGNAL(windowChanged(qint64, qint64)),              this,   SLOT(updateScrollBarFromSlider()));
    connect(m_timeSlider,                       SIGNAL(windowChanged(qint64, qint64)),              this,   SLOT(updateTargetPeriod()));
    connect(m_timeSlider,                       SIGNAL(selectionChanged(qint64, qint64)),           this,   SLOT(at_timeSlider_selectionChanged()));
    connect(m_timeSlider,                       SIGNAL(customContextMenuRequested(const QPointF &, const QPoint &)), this, SLOT(at_timeSlider_customContextMenuRequested(const QPointF &, const QPoint &)));
    connect(m_timeSlider,                       SIGNAL(selectionPressed()),                         this,   SLOT(at_timeSlider_selectionPressed()));
    m_timeSlider->setLineCount(SliderLineCount);
    m_timeSlider->setLineStretch(CurrentLine, 1.5);
    m_timeSlider->setLineStretch(SyncedLine, 1.0);

    connect(m_timeScrollBar,                    SIGNAL(valueChanged(qint64)),                       this,   SLOT(updateSliderFromScrollBar()));
    connect(m_timeScrollBar,                    SIGNAL(pageStepChanged(qint64)),                    this,   SLOT(updateSliderFromScrollBar()));
    connect(m_timeScrollBar,                    SIGNAL(sliderPressed()),                            this,   SLOT(at_timeScrollBar_sliderPressed()));
    connect(m_timeScrollBar,                    SIGNAL(sliderReleased()),                           this,   SLOT(at_timeScrollBar_sliderReleased()));
    m_timeScrollBar->installEventFilter(this);

    updateLines();
    updateScrollBarFromSlider();
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

    if(live) {
        m_timeSlider->setValue(m_timeSlider->maximum());
    } else {
        m_timeSlider->setValue(m_timeSlider->minimum()); // TODO: need to save position here.
    }
    return true;
}

bool QnWorkbenchNavigator::isPlayingSupported() const {
    return m_currentWidget && m_currentWidget->display()->archiveReader();
}

bool QnWorkbenchNavigator::isPlaying() const {
    if(!isPlayingSupported())
        return false;

    return !m_currentWidget->display()->archiveReader()->isMediaPaused();
}

bool QnWorkbenchNavigator::setPlaying(bool playing) {
    if(playing == isPlaying())
        return true;

    if(!isPlayingSupported())
        return false;

    m_pausedOverride = false;

    QnAbstractArchiveReader *reader = m_currentWidget->display()->archiveReader();
    CLCamDisplay *camDisplay = m_currentWidget->display()->camDisplay();
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
        camDisplay->playAudio(false);
        reader->setSingleShotMode(true);

        setSpeed(0.0);
    }

    updatePlaying();
    return true;
}

qreal QnWorkbenchNavigator::speed() const {
    if(!m_currentWidget || m_currentWidget->display()->isStillImage())
        return 0.0;

    if(QnAbstractArchiveReader *reader = m_currentWidget->display()->archiveReader())
        return reader->getSpeed();

    return 1.0;
}

void QnWorkbenchNavigator::setSpeed(qreal speed) {
    speed = qBound(minimalSpeed(), speed, maximalSpeed());
    if(qFuzzyCompare(speed, this->speed()))
        return;

    if(!m_currentWidget)
        return;

    if(QnAbstractArchiveReader *reader = m_currentWidget->display()->archiveReader()) {
        reader->setSpeed(speed);

        setPlaying(!qFuzzyIsNull(speed));

        updateSpeed();
    }
}

qreal QnWorkbenchNavigator::minimalSpeed() const {
    if(!m_currentWidget || m_currentWidget->display()->isStillImage())
        return 0.0;

    if(QnAbstractArchiveReader *reader = m_currentWidget->display()->archiveReader())
        return reader->isNegativeSpeedSupported() ? -16.0 : 0.0;

    return 1.0;
}

qreal QnWorkbenchNavigator::maximalSpeed() const {
    if(!m_currentWidget || m_currentWidget->display()->isStillImage())
        return 0.0;

    if(QnAbstractArchiveReader *reader = m_currentWidget->display()->archiveReader())
        return 16.0;

    return 1.0;
}

void QnWorkbenchNavigator::addSyncedWidget(QnResourceWidget *widget) {
    if (widget == NULL) {
        qnNullWarning(widget);
        return;
    }

    m_syncedWidgets.insert(widget);
    m_syncedResources.insert(widget->resource(), QHashDummyValue());

    updateCurrentWidget();
    updateSyncedPeriods();
}

void QnWorkbenchNavigator::removeSyncedWidget(QnResourceWidget *widget) {
    if(!m_syncedWidgets.remove(widget))
        return;

    /* QHash::erase does nothing when called for container's end, 
     * and is therefore perfectly safe. */
    m_syncedResources.erase(m_syncedResources.find(widget->resource()));
    m_motionIgnoreWidgets.remove(widget);

    if(!widget->isMotionSelectionEmpty())
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

QnWorkbenchNavigator::SliderUserData QnWorkbenchNavigator::currentSliderData() const {
    SliderUserData result;

    result.selectionValid = m_timeSlider->isSelectionValid();
    result.selection = QnTimePeriod(m_timeSlider->selectionStart(), m_timeSlider->selectionEnd() - m_timeSlider->selectionStart());
    
    result.window = QnTimePeriod(m_timeSlider->windowStart(), m_timeSlider->windowEnd() - m_timeSlider->windowStart());
    if((m_currentWidgetFlags & WidgetSupportsLive) && m_timeSlider->windowEnd() == m_timeSlider->maximum())
        result.window.durationMs = -1;

    return result;
}

void QnWorkbenchNavigator::setCurrentSliderData(const SliderUserData &localData) {
    m_timeSlider->setSelectionValid(localData.selectionValid);
    m_timeSlider->setSelection(localData.selection.startTimeMs, localData.selection.startTimeMs + localData.selection.durationMs);

    qint64 windowStart = localData.window.startTimeMs;
    qint64 windowEnd = localData.window.durationMs == -1 ? m_timeSlider->maximum() : localData.window.startTimeMs + localData.window.durationMs;
    if(windowStart < m_timeSlider->minimum()) {
        qint64 delta = m_timeSlider->minimum() - windowStart;
        windowStart += delta;
        windowEnd += delta;
    }
    m_timeSlider->setWindow(windowStart, windowEnd);
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

void QnWorkbenchNavigator::jumpBackward() {
    if(!m_currentWidget)
        return;

    QnAbstractArchiveReader *reader = m_currentWidget->display()->archiveReader();
    if(!reader)
        return;

    m_pausedOverride = false;

    qint64 pos;
    if(QnCachingTimePeriodLoader *loader = this->loader(m_currentWidget)) {
        const QnTimePeriodList fullPeriods = loader->periods(loader->isMotionRegionsEmpty() ? Qn::RecordingRole : Qn::MotionRole);
        const QnTimePeriodList periods = QnTimePeriod::aggregateTimePeriods(fullPeriods, MAX_FRAME_DURATION);
        
        if (!periods.isEmpty()) {
            qint64 currentTime = m_currentWidget->display()->camera()->getCurrentTime();

            if (currentTime == DATETIME_NOW) {
                pos = periods.last().startTimeMs * 1000;
            } else {
                QnTimePeriodList::const_iterator itr = qUpperBound(periods.begin(), periods.end(), currentTime / 1000);
                itr = qMax(itr - 2, periods.begin());
                pos = itr->startTimeMs * 1000;
                if (reader->isReverseMode() && itr->durationMs != -1)
                    pos += itr->durationMs * 1000;
            }
        }
    } else {
        pos = reader->startTime();
    }
    reader->jumpTo(pos, 0);
}

void QnWorkbenchNavigator::jumpForward() {
    if (!m_currentWidget)
        return;

    QnAbstractArchiveReader *reader = m_currentWidget->display()->archiveReader();
    if(!reader)
        return;

    m_pausedOverride = false;

    qint64 pos;
    if(!(m_currentWidgetFlags & WidgetSupportsPeriods)) {
        pos = reader->endTime();
    } else {
        QnCachingTimePeriodLoader *loader = this->loader(m_currentWidget);
        const QnTimePeriodList fullPeriods = loader->periods(loader->isMotionRegionsEmpty() ? Qn::RecordingRole : Qn::MotionRole);
        const QnTimePeriodList periods = QnTimePeriod::aggregateTimePeriods(fullPeriods, MAX_FRAME_DURATION);

        QnTimePeriodList::const_iterator itr = qUpperBound(periods.begin(), periods.end(), m_currentWidget->display()->camera()->getCurrentTime() / 1000);
        if (itr == periods.end() || reader->isReverseMode() && itr->durationMs == -1) {
            pos = DATETIME_NOW;
        } else {
            pos = (itr->startTimeMs + (reader->isReverseMode() ? itr->durationMs : 0)) * 1000;
        }
    }
    reader->jumpTo(pos, 0);

}

void QnWorkbenchNavigator::stepBackward() {
    if(!m_currentWidget)
        return;

    QnAbstractArchiveReader *reader = m_currentWidget->display()->archiveReader();
    if(!reader)
        return;

    m_pausedOverride = false;

    if (!reader->isSkippingFrames() && reader->currentTime() > reader->startTime()) {
        quint64 currentTime = m_currentWidget->display()->camera()->getCurrentTime();

        if (reader->isSingleShotMode())
            m_currentWidget->display()->camDisplay()->playAudio(false); // TODO: wtf?

        reader->previousFrame(currentTime);
    }
}

void QnWorkbenchNavigator::stepForward() {
    if(!m_currentWidget)
        return;

    QnAbstractArchiveReader *reader = m_currentWidget->display()->archiveReader();
    if(!reader)
        return;

    m_pausedOverride = false;

    reader->nextFrame();
}

void QnWorkbenchNavigator::setPlayingTemporary(bool playing) {
    m_currentWidget->display()->archiveReader()->setSingleShotMode(!playing);
    m_currentWidget->display()->camDisplay()->playAudio(playing);
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
    if (m_centralWidget != NULL || !display()->isStreamsSynchronized()) {
        widget = m_centralWidget;
        isCentral = true;
    } else if(m_syncedWidgets.contains(m_currentWidget)) {
        widget = m_currentWidget;
    } else if (m_syncedWidgets.empty()) {
        widget = NULL;
    } else {
        widget = *m_syncedWidgets.begin();
    }

    if (m_currentWidget == widget) {
        if(m_currentWidgetIsCentral != isCentral) {
            m_currentWidgetIsCentral = isCentral;
            updateLines();
        }

        return;
    }

    emit currentWidgetAboutToBeChanged();

    WidgetFlags previousWidgetFlags = m_currentWidgetFlags;
    m_localDataByWidget[m_currentWidget] = currentSliderData();

    if(m_currentWidget) {
        QnAbstractArchiveReader  *archiveReader = m_currentWidget->display()->archiveReader();
        if (archiveReader)
            archiveReader->setPlaybackMask(QnTimePeriodList());
    }

    m_currentWidget = widget;
    if(m_currentWidget) {
        m_currentWidgetFlags = 0;

        bool layoutIsFile = snapshotManager()->flags(m_currentWidget->item()->layout()->resource()) & Qn::LayoutIsFile;

        if(m_currentWidget->resource().dynamicCast<QnSecurityCamResource>())
            m_currentWidgetFlags |= WidgetSupportsLive | WidgetSupportsPeriods | WidgetSupportsSync | WidgetUsesUTC;

        if(layoutIsFile)
            m_currentWidgetFlags |= WidgetSupportsSync;

    } else {
        m_currentWidgetFlags = 0;
    }
    m_currentWidgetLoaded = false;
    m_currentWidgetIsCentral = isCentral;

    updateLines();
    updateSliderOptions();

    updateSliderFromReader();
    if(!((m_currentWidgetFlags & WidgetSupportsSync) && (previousWidgetFlags & WidgetSupportsSync) && display()->isStreamsSynchronized()) && m_currentWidget)
        setCurrentSliderData(m_localDataByWidget.value(m_currentWidget));
    m_timeSlider->finishAnimations();

    updateCurrentPeriods();
    updateLiveSupported();
    updateLive();
    updatePlayingSupported();
    updatePlaying();
    updateSpeedRange();
    updateSpeed();

    emit currentWidgetChanged();
}

void QnWorkbenchNavigator::updateSliderOptions() {
    m_timeSlider->setOption(QnTimeSlider::UseUTC, m_currentWidgetFlags & WidgetUsesUTC);

    bool selectionEditable = !(snapshotManager()->flags(workbench()->currentLayout()->resource()) & Qn::LayoutIsFile);
    m_timeSlider->setOption(QnTimeSlider::SelectionEditable, selectionEditable);
    if(!selectionEditable)
        m_timeSlider->setSelectionValid(false);
}

void QnWorkbenchNavigator::updateSliderFromReader() {
    if (!m_currentWidget)
        return;

    if (m_timeSlider->isSliderDown())
        return;

    QnAbstractArchiveReader *reader = m_currentWidget->display()->archiveReader();
    if (!reader)
        return;

    QnScopedValueRollback<bool> guard(&m_updatingSliderFromReader, true);

    qint64 endTimeUSec = reader->endTime();
    qint64 endTimeMSec = endTimeUSec == DATETIME_NOW ? qnSyncTime->currentMSecsSinceEpoch() : (endTimeUSec == AV_NOPTS_VALUE ? m_timeSlider->maximum() : endTimeUSec / 1000);

    qint64 startTimeUSec = reader->startTime();                       /* vvvvv  If nothing is recorded, set minimum to end - 10s. */
    qint64 startTimeMSec = startTimeUSec == DATETIME_NOW ? endTimeMSec - 10000 : (startTimeUSec == AV_NOPTS_VALUE ? m_timeSlider->minimum() : startTimeUSec / 1000); 

    m_timeSlider->setMinimum(startTimeMSec);
    m_timeSlider->setMaximum(endTimeMSec);

    if(!m_pausedOverride) {
        qint64 timeUSec = reader->isRealTimeSource() ? DATETIME_NOW : m_currentWidget->display()->camera()->getCurrentTime();
        qint64 timeMSec = timeUSec == DATETIME_NOW ? endTimeMSec : (timeUSec == AV_NOPTS_VALUE ? m_timeSlider->value() : timeUSec / 1000);

        m_timeSlider->setValue(timeMSec);

        if(timeUSec != AV_NOPTS_VALUE)
            updateLive();
    }

    /* Fix flags once the reader has loaded. */
    if(!m_currentWidgetLoaded && startTimeUSec != AV_NOPTS_VALUE) {
        if(startTimeUSec > 1000000ll * 60 * 60 * 24 * 365)
            m_currentWidgetFlags |= WidgetUsesUTC;

        m_currentWidgetLoaded = true;

        updateSliderOptions();
        updateTargetPeriod();
    }
}

void QnWorkbenchNavigator::updateTargetPeriod() {
    if (!m_currentWidgetLoaded) 
        return;
    
    /* Update target time period for time period loaders. 
     * If playback is synchronized, do it for all cameras. */
    QnTimePeriod period(m_timeSlider->windowStart(), m_timeSlider->windowEnd() - m_timeSlider->windowStart());
    if(display()->isStreamsSynchronized() && (m_currentWidgetFlags & WidgetSupportsPeriods)) {
        foreach(QnResourceWidget *widget, m_syncedWidgets)
            if(QnCachingTimePeriodLoader *loader = this->loader(widget))
                loader->setTargetPeriod(period);
    } else if(m_currentWidgetFlags & WidgetSupportsPeriods) {
        if(QnCachingTimePeriodLoader *loader = this->loader(m_currentWidget))
            loader->setTargetPeriod(period);
    }
}

void QnWorkbenchNavigator::updateCurrentPeriods() {
    for(int i = 0; i < Qn::TimePeriodRoleCount; i++)
        updateCurrentPeriods(static_cast<Qn::TimePeriodRole>(i));
}

void QnWorkbenchNavigator::updateCurrentPeriods(Qn::TimePeriodRole type) {
    QnTimePeriodList periods;

    if(type == Qn::MotionRole && m_currentWidget && !(m_currentWidget->displayFlags() & QnResourceWidget::DisplayMotionGrid)) {
        /* Use empty periods. */
    } else if(QnCachingTimePeriodLoader *loader = this->loader(m_currentWidget)) {
        periods = loader->periods(type);
    }

    m_timeSlider->setTimePeriods(CurrentLine, type, periods);
}

void QnWorkbenchNavigator::updateSyncedPeriods() {
    for(int i = 0; i < Qn::TimePeriodRoleCount; i++)
        updateSyncedPeriods(static_cast<Qn::TimePeriodRole>(i));
}

void QnWorkbenchNavigator::updateSyncedPeriods(Qn::TimePeriodRole type) {
    QVector<QnTimePeriodList> periodsList;
    foreach(const QnResourceWidget *widget, m_syncedWidgets) {
        if(type == Qn::MotionRole && !(widget->displayFlags() & QnResourceWidget::DisplayMotionGrid)) {
            /* Ignore it. */
        } else if(QnCachingTimePeriodLoader *loader = this->loader(widget->resource())) {
            periodsList.push_back(loader->periods(type));
        }
    }

    QnTimePeriodList periods = QnTimePeriod::mergeTimePeriods(periodsList);

    if (type == Qn::MotionRole) {
        foreach(QnResourceWidget *widget, m_syncedWidgets) {
            QnAbstractArchiveReader  *archiveReader = widget->display()->archiveReader();
            if (archiveReader)
                archiveReader->setPlaybackMask(periods);
        }
    }

    m_timeSlider->setTimePeriods(SyncedLine, type, periods);
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

void QnWorkbenchNavigator::updateLive() {
    bool live = isLive();
    if(m_lastLive == live)
        return;

    m_lastLive = live;
    
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

    if (m_centralWidget)
        if(QnCachingTimePeriodLoader *loader = this->loader(m_centralWidget))
            if(!loader->periods(Qn::RecordingRole).isEmpty())
                resource = m_centralWidget->resource();

    if(!resource) {
        m_timeSlider->setThumbnailsLoader(NULL);
    } else if(!m_timeSlider->thumbnailsLoader() || m_timeSlider->thumbnailsLoader()->resource() != resource) {
        m_thumbnailsLoader.reset(new QnThumbnailsLoader(resource));
        m_timeSlider->setThumbnailsLoader(m_thumbnailsLoader.data());
    }
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
bool QnWorkbenchNavigator::eventFilter(QObject *watched, QEvent *event) {
    if(watched == m_timeScrollBar && event->type() == QEvent::GraphicsSceneWheel) {
        if(m_timeSlider->scene() && m_timeSlider->scene()->sendEvent(m_timeSlider, event))
            return true;
    } else if(watched == m_timeScrollBar && event->type() == QEvent::GraphicsSceneMouseDoubleClick) {
        m_timeSlider->animatedUnzoom();
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
        QnActionParameters(currentTarget(Qn::SliderScope)).withArgument(Qn::TimePeriodParameter, selection)
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
    if(!m_updatingSliderFromReader) {
        QnAbstractArchiveReader *reader = m_currentWidget->display()->archiveReader();
        if (value == DATETIME_NOW) {
            reader->jumpToPreviousFrame(DATETIME_NOW);
        } else {
            if (m_timeSlider->isSliderDown()) {
                reader->jumpTo(value * 1000, 0);
            } else {
                reader->jumpTo(value * 1000, value * 1000); /* Precise seek. */
            }
        }

        updateLive();
    }
}

void QnWorkbenchNavigator::at_timeSlider_sliderPressed() {
    if (!m_currentWidget)
        return;

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

void QnWorkbenchNavigator::at_timeSlider_selectionChanged() {
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

    if(role == Qn::ZoomedRole)
        updateLines();
}

void QnWorkbenchNavigator::at_display_widgetAdded(QnResourceWidget *widget) {
    if(QnSecurityCamResourcePtr cameraResource = widget->resource().dynamicCast<QnSecurityCamResource>()) {
        addSyncedWidget(widget);

        connect(widget, SIGNAL(motionSelectionChanged()), this, SLOT(at_widget_motionSelectionChanged()));
        connect(widget, SIGNAL(displayFlagsChanged()), this, SLOT(at_widget_displayFlagsChanged()));
    }
}

void QnWorkbenchNavigator::at_display_widgetAboutToBeRemoved(QnResourceWidget *widget) {
    if(QnSecurityCamResourcePtr cameraResource = widget->resource().dynamicCast<QnSecurityCamResource>()) {
        disconnect(widget, NULL, this, NULL);

        removeSyncedWidget(widget);
    }
}

void QnWorkbenchNavigator::at_widget_motionSelectionChanged() {
    at_widget_motionSelectionChanged(checked_cast<QnResourceWidget *>(sender()));
}

void QnWorkbenchNavigator::at_widget_motionSelectionChanged(QnResourceWidget *widget) {
    /* We check that the loader can be created (i.e. that the resource is camera) 
     * just to feel safe. */
    if(QnCachingTimePeriodLoader *loader = this->loader(widget->resource()))
        loader->setMotionRegions(widget->motionSelection());
}

void QnWorkbenchNavigator::at_widget_displayFlagsChanged() {
    at_widget_displayFlagsChanged(checked_cast<QnResourceWidget *>(sender()));
}

void QnWorkbenchNavigator::at_widget_displayFlagsChanged(QnResourceWidget *widget) {
    int oldSize = m_motionIgnoreWidgets.size();
    if(widget->displayFlags() & QnResourceWidget::DisplayMotionGrid) {
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

void QnWorkbenchNavigator::at_timeSlider_destroyed() {
    setTimeSlider(NULL);
}

void QnWorkbenchNavigator::at_timeScrollBar_destroyed() {
    setTimeScrollBar(NULL);
}

void QnWorkbenchNavigator::at_timeScrollBar_sliderPressed() {
    m_timeSlider->setOption(QnTimeSlider::AdjustWindowToPosition, false);
}

void QnWorkbenchNavigator::at_timeScrollBar_sliderReleased() {
    m_timeSlider->setOption(QnTimeSlider::AdjustWindowToPosition, true);
}


