#include "workbench_navigator.h"

#include <cassert>

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
#include <ui/actions/action_target_types.h>
#include <ui/graphics/items/resource_widget.h>
#include <ui/graphics/items/controls/time_slider.h>
#include <ui/graphics/instruments/signaling_instrument.h>

#include "workbench_display.h"
#include "workbench_context.h"

#include "plugins/resources/archive/abstract_archive_stream_reader.h"
#include <cstdint> // TODO: remove
#include "libavutil/avutil.h" // TODO: remove
#include <ui/graphics/items/standard/graphics_scroll_bar.h> // TODO: remove

QnWorkbenchNavigator::QnWorkbenchNavigator(QnWorkbenchDisplay *display, QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_timeSlider(NULL),
    m_scrollBar(NULL),
    m_display(display),
    m_centralWidget(NULL),
    m_currentWidget(NULL),
    m_inUpdate(false),
    m_currentWidgetIsCamera(false),
    m_clearSelectionAction(new QAction(this))
{
    assert(display != NULL);

}
    
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

GraphicsScrollBar *QnWorkbenchNavigator::scrollBar() const {
    return m_scrollBar;
}

void QnWorkbenchNavigator::setScroolBar(GraphicsScrollBar *scrollBar) {
    if(m_scrollBar == scrollBar)
        return;

    if(m_scrollBar) {
        disconnect(m_scrollBar, NULL, this, NULL);

        if(isValid())
            deinitialize();
    }

    m_scrollBar = scrollBar;

    if(m_scrollBar) {
        connect(m_scrollBar, SIGNAL(destroyed()), this, SLOT(at_scrollBar_destroyed()));

        if(isValid())
            initialize();
    }
}

bool QnWorkbenchNavigator::isValid() {
    return m_display && m_timeSlider && m_scrollBar;
}

void QnWorkbenchNavigator::initialize() {
    assert(m_display && m_timeSlider);

    connect(m_display,                          SIGNAL(widgetChanged(Qn::ItemRole)),                this,   SLOT(at_display_widgetChanged(Qn::ItemRole)));
    connect(m_display,                          SIGNAL(widgetAdded(QnResourceWidget *)),            this,   SLOT(at_display_widgetAdded(QnResourceWidget *)));
    connect(m_display,                          SIGNAL(widgetAboutToBeRemoved(QnResourceWidget *)), this,   SLOT(at_display_widgetAboutToBeRemoved(QnResourceWidget *)));
    connect(m_display,                          SIGNAL(streamsSynchronizedChanged()),               this,   SLOT(updateCurrentWidget()));
    connect(m_display->beforePaintInstrument(), SIGNAL(activated(QWidget *, QEvent *)),             this,   SLOT(updateSliderFromReader()));
    
    connect(m_timeSlider,                       SIGNAL(valueChanged(qint64)),                       this,   SLOT(at_timeSlider_valueChanged(qint64)));
    connect(m_timeSlider,                       SIGNAL(sliderPressed()),                            this,   SLOT(at_timeSlider_sliderPressed()));
    connect(m_timeSlider,                       SIGNAL(sliderReleased()),                           this,   SLOT(at_timeSlider_sliderReleased()));
    connect(m_timeSlider,                       SIGNAL(rangeChanged(qint64, qint64)),               this,   SLOT(updateScrollBarFromSlider()));
    connect(m_timeSlider,                       SIGNAL(windowChanged(qint64, qint64)),              this,   SLOT(updateScrollBarFromSlider()));
    m_timeSlider->installEventFilter(this);
    m_timeSlider->setLineCount(SliderLineCount);

    connect(m_scrollBar,                        SIGNAL(valueChanged(qint64)),                       this,   SLOT(updateSliderFromScrollBar()));
    connect(m_scrollBar,                        SIGNAL(pageStepChanged(qint64)),                    this,   SLOT(updateSliderFromScrollBar()));
    m_scrollBar->installEventFilter(this);

    updateLineComments();
} 

void QnWorkbenchNavigator::deinitialize() {
    assert(m_display && m_timeSlider);

    disconnect(m_display,                           NULL, this, NULL);
    disconnect(m_display->beforePaintInstrument(),  NULL, this, NULL);
    
    disconnect(m_timeSlider,                        NULL, this, NULL);
    m_timeSlider->removeEventFilter(this);

    disconnect(m_scrollBar,                         NULL, this, NULL);
    m_scrollBar->removeEventFilter(this);

    m_currentWidget = NULL;
    m_currentWidgetIsCamera = false;
}

Qn::ActionScope QnWorkbenchNavigator::currentScope() const {
    return Qn::SliderScope;
}

QVariant QnWorkbenchNavigator::currentTarget(Qn::ActionScope scope) const {
    QnResourceWidgetList result;
    if(m_currentWidget)
        result.push_back(m_currentWidget);
    return QVariant::fromValue<QnResourceWidgetList>(result);
}

void QnWorkbenchNavigator::setCentralWidget(QnResourceWidget *widget) {
    if(m_centralWidget == widget)
        return;

    m_centralWidget = widget;

    updateCurrentWidget();
}

void QnWorkbenchNavigator::addSyncedWidget(QnResourceWidget *widget) {
    if (widget == NULL) {
        qnNullWarning(widget);
        return;
    }

    m_syncedWidgets.insert(widget);
    m_syncedResources.insert(widget->resource(), QHashDummyValue());

    updateCurrentWidget();

    //m_forceTimePeriodLoading = !updateRecPeriodList(true);
}

void QnWorkbenchNavigator::removeSyncedWidget(QnResourceWidget *widget) {
    if(!m_syncedWidgets.remove(widget))
        return;

    /* QHash::erase does nothing when called for container's end, 
     * and is therefore perfectly safe. */
    m_syncedResources.erase(m_syncedResources.find(widget->resource()));

    updateCurrentWidget();

    //m_forceTimePeriodLoading = !updateRecPeriodList(true);

    /*QnNetworkResourcePtr netRes = qSharedPointerDynamicCast<QnNetworkResource>(widget->getDevice());
    if (netRes)
        m_motionPeriodLoader.remove(netRes);

    repaintMotionPeriods();*/
}

void QnWorkbenchNavigator::updateCurrentWidget() {
    if (m_centralWidget != NULL || !m_display->isStreamsSynchronized()) {
        setCurrentWidget(m_centralWidget);
        return;
    }

    if (m_syncedWidgets.contains(m_currentWidget))
        return;

    if (m_syncedWidgets.empty()) {
        setCurrentWidget(NULL);
        return;
    }

    setCurrentWidget(*m_syncedWidgets.begin());
}

QnResourceWidget *QnWorkbenchNavigator::currentWidget() {
    return m_currentWidget;
}

void QnWorkbenchNavigator::setCurrentWidget(QnResourceWidget *widget) {
    if (m_currentWidget == widget)
        return;

    m_currentWidget = widget;
    if(m_currentWidget) {
        m_currentWidgetIsCamera = m_currentWidget->resource().dynamicCast<QnSecurityCamResource>();
    } else {
        m_currentWidgetIsCamera = false;
    }

    updateLineComments();
    m_timeSlider->setOption(QnTimeSlider::UseUTC, m_currentWidgetIsCamera);

    updateSliderFromReader();
    m_timeSlider->finishAnimations();

    updateCurrentPeriods();

#if 0
    m_timeSlider->resetSelectionRange();

    bool zoomResetNeeded = !(m_syncedWidgets.contains(widget) && m_syncedWidgets.contains(m_currentWidget) && m_syncButton->isChecked());

    if (m_camera) {
        disconnect(m_camera->getCamDisplay(), 0, this, 0);

        m_zoomByCamera[m_camera] = m_timeSlider->scalingFactor();
    }

    m_camera = widget;

    if (m_camera)
    {
        // Use QueuedConnection connection because of deadlock. Signal can came from before jump and speed changing can call pauseMedia (because of speed slider animation).
        connect(m_camera->getCamDisplay(), SIGNAL(liveMode(bool)), this, SLOT(onLiveModeChanged(bool)), Qt::QueuedConnection);

        QnAbstractArchiveReader *reader = dynamic_cast<QnAbstractArchiveReader*>(m_camera->getStreamreader());


        if (reader) {
            setPlaying(!reader->isMediaPaused());
            m_timeSlider->setLiveMode(reader->isRealTimeSource());
            m_speedSlider->setSpeed(reader->getSpeed());
        }
        else
            setPlaying(true);
        if (!m_syncButton->isChecked()) {
            updateRecPeriodList(true);
            repaintMotionPeriods();
        }

    }
    else
    {
        m_timeSlider->setScalingFactor(0);
    }

    if(zoomResetNeeded) {
        updateSlider();

        m_timeSlider->setScalingFactor(m_zoomByCamera.value(m_camera, 0.0));
    }

    m_syncButton->setEnabled(m_reserveCameras.contains(widget));
    m_liveButton->setEnabled(m_reserveCameras.contains(widget));

#endif

    emit currentWidgetChanged();
}

QnCachingTimePeriodLoader *QnWorkbenchNavigator::loader(const QnResourcePtr &resource) {
    QHash<QnResourcePtr, QnCachingTimePeriodLoader *>::const_iterator pos = m_loaderByResource.find(resource);
    if(pos != m_loaderByResource.end())
        return *pos;

    QnCachingTimePeriodLoader *loader = QnCachingTimePeriodLoader::newInstance(resource, this);
    if(loader)
        connect(loader, SIGNAL(periodsChanged(Qn::TimePeriodType)), this, SLOT(at_loader_periodsChanged(Qn::TimePeriodType)));

    m_loaderByResource[resource] = loader;
    return loader;
}

void QnWorkbenchNavigator::updateSliderFromReader() {
    if (!m_currentWidget)
        return;

    if (m_timeSlider->isSliderDown())
        return;

    QnAbstractArchiveReader *reader = m_currentWidget->display()->archiveReader();;
    if (!reader)
        return;

    QnScopedValueRollback<bool> guard(&m_inUpdate, true);

    qint64 startTimeUSec = reader->startTime();
    qint64 endTimeUSec = reader->endTime();
    if (startTimeUSec != AV_NOPTS_VALUE && endTimeUSec != AV_NOPTS_VALUE) {// TODO: rename AV_NOPTS_VALUE to something more sane.
        qint64 currentMSecsSinceEpoch = 0;
        if(startTimeUSec == DATETIME_NOW || endTimeUSec == DATETIME_NOW)
            currentMSecsSinceEpoch = qnSyncTime->currentMSecsSinceEpoch();

        qint64 startTimeMSec = startTimeUSec != DATETIME_NOW ? startTimeUSec / 1000 : currentMSecsSinceEpoch - 10000; /* If nothing is recorded, set minimum to live - 10s. */
        qint64 endTimeMSec = endTimeUSec != DATETIME_NOW ? endTimeUSec / 1000 : currentMSecsSinceEpoch;

        m_timeSlider->setMinimum(startTimeMSec);
        m_timeSlider->setMaximum(endTimeMSec);

        qint64 timeUSec = m_currentWidget->display()->camera()->getCurrentTime();
        if (timeUSec != AV_NOPTS_VALUE)
            m_timeSlider->setValue(timeUSec != DATETIME_NOW ? timeUSec / 1000 : currentMSecsSinceEpoch);

        QnTimePeriod period(startTimeMSec, endTimeMSec - startTimeMSec);
        foreach(QnResourceWidget *widget, m_syncedWidgets)
            loader(widget->resource())->setTargetPeriod(period);

        //m_forceTimePeriodLoading = !updateRecPeriodList(m_forceTimePeriodLoading); // if period does not loaded yet, force loading
    }


    /*if(!reader->isMediaPaused() && (m_camera->getCamDisplay()->isRealTimeSource() || m_timeSlider->currentValue() == DATETIME_NOW)) {
        m_liveButton->setChecked(true);
        m_forwardButton->setEnabled(false);
        m_stepForwardButton->setEnabled(false);
    } else {
        m_liveButton->setChecked(false);
        m_forwardButton->setEnabled(true);
        m_stepForwardButton->setEnabled(true);
    }*/
}

void QnWorkbenchNavigator::updateCurrentPeriods() {
    for(int i = 0; i < Qn::TimePeriodTypeCount; i++)
        updateCurrentPeriods(static_cast<Qn::TimePeriodType>(i));
}

void QnWorkbenchNavigator::updateCurrentPeriods(Qn::TimePeriodType type) {
    QnCachingTimePeriodLoader *loader = !m_currentWidget ? NULL : this->loader(m_currentWidget->resource());
    if(loader) {
        m_timeSlider->setTimePeriods(CurrentLine, type, loader->periods(type));
    } else {
        m_timeSlider->setTimePeriods(CurrentLine, type, QnTimePeriodList());
    }
}

void QnWorkbenchNavigator::updateSyncedPeriods(Qn::TimePeriodType type) {
    QVector<QnTimePeriodList> periods;
    foreach(const QnResourcePtr &resource, m_syncedResources.uniqueKeys())
        periods.push_back(loader(resource)->periods(type));

    m_timeSlider->setTimePeriods(SyncedLine, type, QnTimePeriod::mergeTimePeriods(periods));
}

void QnWorkbenchNavigator::updateLineComments() {
    if(m_currentWidgetIsCamera) {
        m_timeSlider->setLineComment(CurrentLine, m_currentWidget->resource()->getName());
        m_timeSlider->setLineComment(SyncedLine, tr("All Cameras"));
    } else {
        m_timeSlider->setLineComment(CurrentLine, QString());
        m_timeSlider->setLineComment(SyncedLine, QString());
    }
}

void QnWorkbenchNavigator::updateSliderFromScrollBar() {
    m_timeSlider->setWindow(m_scrollBar->value(), m_scrollBar->value() + m_scrollBar->pageStep());
}

void QnWorkbenchNavigator::updateScrollBarFromSlider() {
    qint64 windowSize = m_timeSlider->windowEnd() - m_timeSlider->windowStart();

    m_scrollBar->setRange(m_timeSlider->minimum(), m_timeSlider->maximum() - windowSize);
    m_scrollBar->setValue(m_timeSlider->windowStart());
    m_scrollBar->setPageStep(windowSize);
}


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
bool QnWorkbenchNavigator::eventFilter(QObject *watched, QEvent *event) {
    if(watched == m_timeSlider && event->type() == QEvent::GraphicsSceneContextMenu) {
        at_timeSlider_contextMenuEvent(static_cast<QGraphicsSceneContextMenuEvent *>(event));
        return true;
    } else if(watched == m_scrollBar && event->type() == QEvent::GraphicsSceneWheel) {
        if(m_timeSlider->scene() && m_timeSlider->scene()->sendEvent(m_timeSlider, event))
            return true;
    }

    return base_type::eventFilter(watched, event);
}

void QnWorkbenchNavigator::at_timeSlider_contextMenuEvent(QGraphicsSceneContextMenuEvent *event) {
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

    /* Add slider-local actions to the menu. */
    manager->redirectAction(menu.data(), Qn::ClearTimeSelectionAction, m_clearSelectionAction);

    if(menu->isEmpty())
        return;

    /* Run menu. */
    QAction *action = menu->exec(QCursor::pos());

    /* Process tree-local actions. */
    if(action == m_clearSelectionAction)
        m_timeSlider->setSelectionValid(false);
}

void QnWorkbenchNavigator::at_loader_periodsChanged(Qn::TimePeriodType type) {
    at_loader_periodsChanged(checked_cast<QnCachingTimePeriodLoader *>(sender()), type);
}

void QnWorkbenchNavigator::at_loader_periodsChanged(QnCachingTimePeriodLoader *loader, Qn::TimePeriodType type) {
    QnResourcePtr resource = loader->resource();

    if(m_currentWidget && m_currentWidget->resource() == resource)
        updateCurrentPeriods(type);
    
    if(m_syncedResources.contains(resource))
        updateSyncedPeriods(type);
}

void QnWorkbenchNavigator::at_timeSlider_valueChanged(qint64 value) {
    if(!m_currentWidget)
        return;

    if(value == m_timeSlider->maximum() && m_timeSlider->minimum() > 0)
        value = DATETIME_NOW;

    /* Update tool tip format. */
    if (value == DATETIME_NOW) {
        m_timeSlider->setToolTipFormat(tr("'Live'", "LIVE_TOOL_TIP_FORMAT"));
    } else {
        if (m_currentWidgetIsCamera) {
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
    if(!m_inUpdate) {
        QnAbstractArchiveReader *reader = m_currentWidget->display()->archiveReader();
        if (value == DATETIME_NOW) {
            reader->jumpToPreviousFrame(DATETIME_NOW);
        } else {
            if (m_timeSlider->isSliderDown()) {
                reader->jumpTo(value * 1000, 0);
            } else {
                reader->jumpToPreviousFrame(value * 1000);
            }
        }
    }
    
    //updateRecPeriodList(false);
}

void QnWorkbenchNavigator::at_timeSlider_sliderPressed() {
    if (!m_currentWidget)
        return;

    m_currentWidget->display()->archiveReader()->setSingleShotMode(true);
    m_currentWidget->display()->camDisplay()->playAudio(false);

    m_wasPlaying = !m_currentWidget->display()->archiveReader()->isMediaPaused();
}

void QnWorkbenchNavigator::at_timeSlider_sliderReleased() {
    if (!m_currentWidget)
        return;

    /*bool isCamera = m_currentWidget->resource().dynamicCast<QnSecurityCamResource>();
    if (!isCamera) {
        // Disable precise seek via network to reduce network flood. 
        //smartSeek(m_timeSlider->currentValue());
    }*/
    if (m_wasPlaying) {
        m_currentWidget->display()->archiveReader()->setSingleShotMode(false);
        m_currentWidget->display()->camDisplay()->playAudio(true);
    }
}

void QnWorkbenchNavigator::at_display_widgetChanged(Qn::ItemRole role) {
    if(role == Qn::CentralRole)
        setCentralWidget(m_display->widget(role));
}

void QnWorkbenchNavigator::at_display_widgetAdded(QnResourceWidget *widget) {
    if(QnSecurityCamResourcePtr cameraResource = widget->resource().dynamicCast<QnSecurityCamResource>()) {
        addSyncedWidget(widget);

        connect(widget, SIGNAL(motionSelectionChanged()), this, SLOT(at_widget_motionSelectionChanged()));
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

void QnWorkbenchNavigator::at_timeSlider_destroyed() {
    setTimeSlider(NULL);
}

void QnWorkbenchNavigator::at_scrollBar_destroyed() {
    setScroolBar(NULL);
}





