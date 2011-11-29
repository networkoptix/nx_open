#include "render_watch_mixin.h"
#include <ui/workbench/workbench_display.h>
#include <ui/graphics/items/resource_widget_renderer.h>
#include <ui/graphics/items/resource_widget.h>
#include <ui/graphics/instruments/signalinginstrument.h>
#include <ui/graphics/instruments/instrumentmanager.h>
#include <ui/graphics/instruments/forwardinginstrument.h>
#include <camera/abstractrenderer.h>
#include <utils/common/warnings.h>

QnRenderWatchMixin::QnRenderWatchMixin(QnWorkbenchDisplay *display, QObject *parent):
    QObject(parent) 
{
    if(display == NULL) {
        qnNullWarning(display);
        return;
    }

    /* Connect to display. */
    connect(display,    SIGNAL(widgetAdded(QnResourceWidget *)),            this,   SLOT(at_display_widgetAdded(QnResourceWidget *)));
    connect(display,    SIGNAL(widgetAboutToBeRemoved(QnResourceWidget *)), this,   SLOT(at_display_widgetAboutToBeRemoved(QnResourceWidget *)));

    /* Set up instruments. */
    Instrument::EventTypeSet paintEventTypes = Instrument::makeSet(QEvent::Paint);
    SignalingInstrument *beforeDisplayInstrument = new SignalingInstrument(Instrument::VIEWPORT, paintEventTypes, this);
    SignalingInstrument *afterDisplayInstrument = new SignalingInstrument(Instrument::VIEWPORT, paintEventTypes, this);

    InstrumentManager *manager = display->instrumentManager();
    manager->installInstrument(beforeDisplayInstrument, InstrumentManager::INSTALL_BEFORE, display->paintForwardingInstrument());
    manager->installInstrument(afterDisplayInstrument,  InstrumentManager::INSTALL_AFTER, display->paintForwardingInstrument());

    connect(beforeDisplayInstrument, SIGNAL(activated(QWidget *, QEvent *)), this, SLOT(startDisplay()));
    connect(afterDisplayInstrument,  SIGNAL(activated(QWidget *, QEvent *)), this, SLOT(finishDisplay()));
}

QnRenderWatchMixin::QnRenderWatchMixin(QObject *parent): 
    QObject(parent)
{}

void QnRenderWatchMixin::registerRenderer(CLAbstractRenderer *renderer, QObject *lifetime) {
    if(renderer == NULL) {
        qnNullWarning(renderer);
        return;
    }

    if(lifetime == NULL) {
        qnNullWarning(lifetime);
        return;
    }

    if(m_infoByRenderer.contains(renderer)) {
        qnWarning("Renderer re-registration is not allowed.");
        return;
    }

    m_infoByRenderer[renderer] = Info(0, false, lifetime);
    m_rendererByLifetime[lifetime] = renderer;

    connect(lifetime, SIGNAL(destroyed()), this, SLOT(at_lifetime_destroyed()));
}

void QnRenderWatchMixin::unregisterRenderer(CLAbstractRenderer *renderer) {
    if(renderer == NULL) {
        qnNullWarning(renderer);
        return;
    }

    if(!m_infoByRenderer.contains(renderer))
        return; /* It's OK to unregister a renderer that is not there. */

    QObject *lifetime = m_infoByRenderer[renderer].lifetime;
    disconnect(lifetime, NULL, this, NULL);

    m_rendererByLifetime.remove(lifetime);
    m_infoByRenderer.remove(renderer);
}

void QnRenderWatchMixin::at_lifetime_destroyed() {
    unregisterRenderer(m_rendererByLifetime[sender()]);
}

void QnRenderWatchMixin::startDisplay() {
    for(QHash<CLAbstractRenderer *, Info>::iterator pos = m_infoByRenderer.begin(); pos != m_infoByRenderer.end(); pos++)
        pos->displayCounter = pos.key()->displayCounter();
}

void QnRenderWatchMixin::finishDisplay() {
    for(QHash<CLAbstractRenderer *, Info>::iterator pos = m_infoByRenderer.begin(); pos != m_infoByRenderer.end(); pos++) {
        bool displayed = pos->displayCounter != pos.key()->displayCounter();

        if(displayed && !pos->displaying) {
            pos->displaying = true;

            emit displayingStateChanged(pos.key(), pos->displaying);
        } else if(!displayed && pos->displaying) {
            pos->displaying = false;

            emit displayingStateChanged(pos.key(), pos->displaying);
        }
    }
}

void QnRenderWatchMixin::at_display_widgetAdded(QnResourceWidget *widget) {
    if(widget->renderer() == NULL) 
        return;

    registerRenderer(widget->renderer(), widget->renderer());
}

void QnRenderWatchMixin::at_display_widgetAboutToBeRemoved(QnResourceWidget *widget) {
    if(widget->renderer() == NULL) 
        return;

    unregisterRenderer(widget->renderer());
}

