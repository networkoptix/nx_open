#include "workbench_render_watcher.h"

#include <utils/common/warnings.h>
#include <utils/common/checked_cast.h>

#include <camera/abstract_renderer.h>

#include <ui/workbench/workbench_display.h>
#include <ui/graphics/items/resource/resource_widget_renderer.h>
#include <ui/graphics/items/resource/media_resource_widget.h>
#include <ui/graphics/instruments/signaling_instrument.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/instruments/forwarding_instrument.h>


QnWorkbenchRenderWatcher::QnWorkbenchRenderWatcher(QObject *parent):
    QObject(parent),
    QnWorkbenchContextAware(parent)
{
    /* Connect to display. */
    connect(display(),                          SIGNAL(widgetAdded(QnResourceWidget *)),            this,   SLOT(registerWidget(QnResourceWidget *)));
    connect(display(),                          SIGNAL(widgetAboutToBeRemoved(QnResourceWidget *)), this,   SLOT(unregisterWidget(QnResourceWidget *)));
    connect(display()->beforePaintInstrument(), SIGNAL(activated(QWidget *, QEvent *)),             this,   SLOT(at_beforePaintInstrument_activated()));
    connect(display()->afterPaintInstrument(),  SIGNAL(activated(QWidget *, QEvent *)),             this,   SLOT(at_afterPaintInstrument_activated()));
}

QnWorkbenchRenderWatcher::~QnWorkbenchRenderWatcher() {
    return;
}

bool QnWorkbenchRenderWatcher::isDisplaying(QnResourceWidget *widget) const {
    return m_dataByWidget.value(widget).displaying;
}

bool QnWorkbenchRenderWatcher::isDisplaying(QnResourceDisplay *display) const {
    return m_countByDisplay.value(display, 0) > 0;
}

void QnWorkbenchRenderWatcher::setDisplaying(QnResourceWidget *widget, bool displaying) {
    WidgetData &data = m_dataByWidget[widget];
    if(data.displaying == displaying)
        return;

    data.displaying = displaying;

    emit widgetChanged(widget);

    if(data.display) {
        /* Not all widgets have an associated display. */
        if(displaying) {
            if(++m_countByDisplay[data.display] == 1)
                emit displayChanged(data.display);
        } else {
            if(--m_countByDisplay[data.display] == 0) {
                m_countByDisplay.remove(data.display);
                emit displayChanged(data.display);
            }
        }
    }
}

void QnWorkbenchRenderWatcher::registerWidget(QnResourceWidget *widget) {
    if(!widget) {
        qnNullWarning(widget);
        return;
    }

    if(m_dataByWidget.contains(widget)) {
        qnWarning("Widget re-registration is not allowed.");
        return;
    }

    connect(widget, SIGNAL(painted()), this, SLOT(at_widget_painted()));

    QnResourceDisplay *display = NULL;
    if(QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget)) {
        display = mediaWidget->display().data();

        connect(widget, SIGNAL(displayChanged()), this, SLOT(at_widget_displayChanged()));
    }

    m_dataByWidget[widget] = WidgetData(false, display);
}

void QnWorkbenchRenderWatcher::unregisterWidget(QnResourceWidget *widget) {
    if(!widget) {
        qnNullWarning(widget);
        return;
    }

    if(!m_dataByWidget.contains(widget))
        return; /* It's OK to unregister a widget that is not there. */

    disconnect(widget, NULL, this, NULL);

    m_dataByWidget.remove(widget);
}



// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchRenderWatcher::at_beforePaintInstrument_activated() {
    for(QHash<QnResourceWidget *, WidgetData>::iterator pos = m_dataByWidget.begin(); pos != m_dataByWidget.end(); pos++)
        pos->newDisplaying = false;
}

void QnWorkbenchRenderWatcher::at_afterPaintInstrument_activated() {
    for(QHash<QnResourceWidget *, WidgetData>::iterator pos = m_dataByWidget.begin(); pos != m_dataByWidget.end(); pos++)
        setDisplaying(pos.key(), pos->newDisplaying);
}

void QnWorkbenchRenderWatcher::at_widget_displayChanged() {
    at_widget_displayChanged(checked_cast<QnMediaResourceWidget *>(sender()));
}

void QnWorkbenchRenderWatcher::at_widget_displayChanged(QnMediaResourceWidget *widget) {
    bool isDisplaying = this->isDisplaying(widget);
    
    setDisplaying(widget, false);
    m_dataByWidget[widget].display = widget->display().data();
    setDisplaying(widget, isDisplaying);
}

void QnWorkbenchRenderWatcher::at_widget_painted() {
    at_widget_painted(checked_cast<QnResourceWidget *>(sender()));
}

void QnWorkbenchRenderWatcher::at_widget_painted(QnResourceWidget *widget) {
    m_dataByWidget[widget].newDisplaying = true;
}




