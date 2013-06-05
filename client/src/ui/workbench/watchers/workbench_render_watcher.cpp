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
    connect(widget, SIGNAL(displayChanged()), this, SLOT(at_widget_displayChanged()));

    m_dataByWidget[widget] = Data(false);
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
    for(QHash<QnResourceWidget *, Data>::iterator pos = m_dataByWidget.begin(); pos != m_dataByWidget.end(); pos++)
        pos->newDisplaying = false;
}

void QnWorkbenchRenderWatcher::at_afterPaintInstrument_activated() {
    for(QHash<QnResourceWidget *, Data>::iterator pos = m_dataByWidget.begin(); pos != m_dataByWidget.end(); pos++) {
        if(pos->displaying != pos->newDisplaying) {
            pos->newDisplaying = pos->displaying;
            emit displayingChanged(pos.key());
        }
    }
}

void QnWorkbenchRenderWatcher::at_widget_displayChanged() {
    at_widget_displayChanged(checked_cast<QnResourceWidget *>(sender()));
}

void QnWorkbenchRenderWatcher::at_widget_displayChanged(QnResourceWidget *widget) {
    
}

void QnWorkbenchRenderWatcher::at_widget_painted() {
    at_widget_painted(checked_cast<QnResourceWidget *>(sender()));
}

void QnWorkbenchRenderWatcher::at_widget_painted(QnResourceWidget *widget) {
    m_dataByWidget[widget].newDisplaying = true;
}




