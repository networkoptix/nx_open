#include "workbench_navigator.h"

#include <cassert>

#include <utils/common/util.h>
#include <utils/common/synctime.h>
#include <utils/common/scoped_value_rollback.h>

#include <core/resource/camera_resource.h>

#include <camera/resource_display.h>
#include <camera/camera.h>
#include <ui/graphics/items/resource_widget.h>
#include <ui/graphics/items/controls/time_slider.h>
#include <ui/graphics/instruments/signaling_instrument.h>

#include "workbench_display.h"
#include "plugins/resources/archive/abstract_archive_stream_reader.h"


#include <cstdint> // TODO: remove
#include "libavutil/avutil.h" // TODO: remove



QnWorkbenchNavigator::QnWorkbenchNavigator(QnWorkbenchDisplay *display, QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent),
    m_timeSlider(NULL),
    m_display(display),
    m_centralWidget(NULL),
    m_currentWidget(NULL),
    m_inUpdate(false)
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

        deinitialize();
    }

    m_timeSlider = timeSlider;

    if(m_timeSlider) {
        connect(m_timeSlider, SIGNAL(destroyed()), this, SLOT(at_timeSlider_destroyed()));

        initialize();
    }
}

void QnWorkbenchNavigator::initialize() {
    assert(m_display && m_timeSlider);

    connect(m_display,                          SIGNAL(widgetChanged(Qn::ItemRole)),                this,   SLOT(at_display_widgetChanged(Qn::ItemRole)));
    connect(m_display,                          SIGNAL(widgetAdded(QnResourceWidget *)),            this,   SLOT(at_display_widgetAdded(QnResourceWidget *)));
    connect(m_display,                          SIGNAL(widgetAboutToBeRemoved(QnResourceWidget *)), this,   SLOT(at_display_widgetAboutToBeRemoved(QnResourceWidget *)));
    connect(m_display,                          SIGNAL(streamsSynchronizedChanged()),               this,   SLOT(updateCurrentWidget()));
    connect(m_display->beforePaintInstrument(), SIGNAL(activated(QWidget *, QEvent *)),             this,   SLOT(updateSlider()));
    connect(m_timeSlider,                       SIGNAL(valueChanged(qint64)),                       this,   SLOT(at_timeSlider_valueChanged(qint64)));
} 

void QnWorkbenchNavigator::deinitialize() {
    assert(m_display && m_timeSlider);

    disconnect(m_display,                           NULL, this, NULL);
    disconnect(m_display->beforePaintInstrument(),  NULL, this, NULL);
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

    updateCurrentWidget();
    //m_forceTimePeriodLoading = !updateRecPeriodList(true);
}

void QnWorkbenchNavigator::removeSyncedWidget(QnResourceWidget *widget) {
    if(!m_syncedWidgets.remove(widget))
        return;

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


void QnWorkbenchNavigator::updateSlider() {
    if (!m_currentWidget)
        return;

    if (m_timeSlider->isSliderDown())
        return;

    QnAbstractArchiveReader *reader = m_currentWidget->display()->archiveReader();;
    if (!reader)
        return;

    QnScopedValueRollback<bool> guard(&m_inUpdate, true);

    qint64 startTimeUsec = reader->startTime();
    qint64 endTimeUsec = reader->endTime();
    if (startTimeUsec != AV_NOPTS_VALUE && endTimeUsec != AV_NOPTS_VALUE) {// TODO: rename AV_NOPTS_VALUE to something more sane.
        qint64 currentMSecsSinceEpoch = 0;
        if(startTimeUsec == DATETIME_NOW || endTimeUsec == DATETIME_NOW)
            currentMSecsSinceEpoch = qnSyncTime->currentMSecsSinceEpoch();

        m_timeSlider->setMinimum(startTimeUsec != DATETIME_NOW ? startTimeUsec / 1000 : currentMSecsSinceEpoch - 10000); /* If nothing is recorded, set minimum to live - 10s. */
        m_timeSlider->setMaximum(endTimeUsec != DATETIME_NOW ? endTimeUsec / 1000 : currentMSecsSinceEpoch);

        qint64 timeUsec = m_currentWidget->display()->camera()->getCurrentTime();
        if (timeUsec != AV_NOPTS_VALUE)
            m_timeSlider->setValue(timeUsec != DATETIME_NOW ? timeUsec / 1000 : currentMSecsSinceEpoch);

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

// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchNavigator::at_timeSlider_valueChanged(qint64 value) {
    if(!m_currentWidget)
        return;

    if(value == m_timeSlider->maximum() && m_timeSlider->minimum() > 0)
        value = DATETIME_NOW;

    /* Update tool tip format. */
    if (value == DATETIME_NOW) {
        m_timeSlider->setToolTipFormat(tr("'Live'", "LIVE_TOOL_TIP_FORMAT"));
    } else {
        if (m_timeSlider->minimum() == 0) {
            if(m_timeSlider->maximum() >= 60ll * 60ll * 1000ll) { /* Longer than 1 hour. */
                m_timeSlider->setToolTipFormat(tr("hh:mm:ss", "LONG_TOOL_TIP_FORMAT"));
            } else {
                m_timeSlider->setToolTipFormat(tr("mm:ss", "SHORT_TOOL_TIP_FORMAT"));
            }
        } else {
            m_timeSlider->setToolTipFormat(tr("yyyy MMM dd\nhh:mm:ss", "CAMERA_TOOL_TIP_FORMAT"));
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

        connect(widget, SIGNAL(motionRegionSelected(QnResourcePtr, QnAbstractArchiveReader *, QList<QRegion>)), this, SLOT(at_widget_motionRegionSelected(QnResourcePtr, QnAbstractArchiveReader*, QList<QRegion>)));
    }
}

void QnWorkbenchNavigator::at_display_widgetAboutToBeRemoved(QnResourceWidget *widget) {
    if(QnSecurityCamResourcePtr cameraResource = widget->resource().dynamicCast<QnSecurityCamResource>()) {
        disconnect(widget, NULL, this, NULL);

        removeSyncedWidget(widget);
    }
}

void QnWorkbenchNavigator::at_widget_motionRegionSelected(const QnResourcePtr &resource, QnAbstractArchiveReader *reader, const QList<QRegion> &selection) {
    return;
}

void QnWorkbenchNavigator::at_timeSlider_destroyed() {
    setTimeSlider(NULL);
}

