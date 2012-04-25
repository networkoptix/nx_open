#include "workbench_render_watcher.h"
#include <ui/workbench/workbench_display.h>
#include <ui/graphics/items/resource_widget_renderer.h>
#include <ui/graphics/items/resource_widget.h>
#include <ui/graphics/instruments/signaling_instrument.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/instruments/forwarding_instrument.h>
#include <camera/abstractrenderer.h>
#include <utils/common/warnings.h>

QnWorkbenchRenderWatcher::QnWorkbenchRenderWatcher(QnWorkbenchDisplay *display, QObject *parent):
    QObject(parent) 
{
    if(display == NULL) {
        qnNullWarning(display);
        return;
    }

    /* Connect to display. */
    connect(display,                            SIGNAL(widgetAdded(QnResourceWidget *)),            this,   SLOT(at_display_widgetAdded(QnResourceWidget *)));
    connect(display,                            SIGNAL(widgetAboutToBeRemoved(QnResourceWidget *)), this,   SLOT(at_display_widgetAboutToBeRemoved(QnResourceWidget *)));
    connect(display->beforePaintInstrument(),   SIGNAL(activated(QWidget *, QEvent *)),             this, SLOT(startDisplay()));
    connect(display->afterPaintInstrument(),    SIGNAL(activated(QWidget *, QEvent *)),             this, SLOT(finishDisplay()));
}

QnWorkbenchRenderWatcher::QnWorkbenchRenderWatcher(QObject *parent): 
    QObject(parent)
{}

void QnWorkbenchRenderWatcher::registerRenderer(QnAbstractRenderer *renderer, QObject *lifetime) {
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

void QnWorkbenchRenderWatcher::unregisterRenderer(QnAbstractRenderer *renderer) {
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

void QnWorkbenchRenderWatcher::at_lifetime_destroyed() {
    unregisterRenderer(m_rendererByLifetime[sender()]);
}

void QnWorkbenchRenderWatcher::startDisplay() {
    for(QHash<QnAbstractRenderer *, Info>::iterator pos = m_infoByRenderer.begin(); pos != m_infoByRenderer.end(); pos++)
        pos->displayCounter = pos.key()->displayCounter();
}

void QnWorkbenchRenderWatcher::finishDisplay() {
    for(QHash<QnAbstractRenderer *, Info>::iterator pos = m_infoByRenderer.begin(); pos != m_infoByRenderer.end(); pos++) {
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

void QnWorkbenchRenderWatcher::at_display_widgetAdded(QnResourceWidget *widget) {
    if(widget->renderer() == NULL) 
        return;

    registerRenderer(widget->renderer(), widget->renderer());
}

void QnWorkbenchRenderWatcher::at_display_widgetAboutToBeRemoved(QnResourceWidget *widget) {
    if(widget->renderer() == NULL) 
        return;

    unregisterRenderer(widget->renderer());
}

