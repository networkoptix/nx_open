#include "workbench_render_watcher.h"

#include <utils/common/warnings.h>

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
    connect(display(),                          SIGNAL(widgetAdded(QnResourceWidget *)),            this,   SLOT(at_display_widgetAdded(QnResourceWidget *)));
    connect(display(),                          SIGNAL(widgetAboutToBeRemoved(QnResourceWidget *)), this,   SLOT(at_display_widgetAboutToBeRemoved(QnResourceWidget *)));
    connect(display()->beforePaintInstrument(), SIGNAL(activated(QWidget *, QEvent *)),             this,   SLOT(at_beforePaintInstrument_activated()));
    connect(display()->afterPaintInstrument(),  SIGNAL(activated(QWidget *, QEvent *)),             this,   SLOT(at_afterPaintInstrument_activated()));
}

QnWorkbenchRenderWatcher::QnWorkbenchRenderWatcher(QObject *parent): 
    QObject(parent)
{}

void QnWorkbenchRenderWatcher::registerRenderer(QnAbstractRenderer *renderer) {
    if(renderer == NULL) {
        qnNullWarning(renderer);
        return;
    }

    QObject *lifetime = dynamic_cast<QObject *>(renderer);
    if(lifetime == NULL) {
        qnWarning("Given renderer must be derived from QObject.");
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


// -------------------------------------------------------------------------- //
// Handlers
// -------------------------------------------------------------------------- //
void QnWorkbenchRenderWatcher::at_beforePaintInstrument_activated() {
    for(QHash<QnAbstractRenderer *, Info>::iterator pos = m_infoByRenderer.begin(); pos != m_infoByRenderer.end(); pos++)
        pos->displayCounter = pos.key()->displayCounter();
}

void QnWorkbenchRenderWatcher::at_afterPaintInstrument_activated() {
    for(QHash<QnAbstractRenderer *, Info>::iterator pos = m_infoByRenderer.begin(); pos != m_infoByRenderer.end(); pos++) {
        bool displayed = pos->displayCounter != pos.key()->displayCounter();

        if(displayed && !pos->displaying) {
            pos->displaying = true;

            emit displayingChanged(pos.key(), pos->displaying);
        } else if(!displayed && pos->displaying) {
            pos->displaying = false;

            emit displayingChanged(pos.key(), pos->displaying);
        }
    }
}

void QnWorkbenchRenderWatcher::at_display_widgetAdded(QnResourceWidget *widget) {
    QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget);
    if(!mediaWidget)
        return;

    if(mediaWidget->renderer() == NULL) 
        return;

    registerRenderer(mediaWidget->renderer());
}

void QnWorkbenchRenderWatcher::at_display_widgetAboutToBeRemoved(QnResourceWidget *widget) {
    QnMediaResourceWidget *mediaWidget = dynamic_cast<QnMediaResourceWidget *>(widget);
    if(!mediaWidget)
        return;

    if(mediaWidget->renderer() == NULL) 
        return;

    unregisterRenderer(mediaWidget->renderer());
}

void QnWorkbenchRenderWatcher::at_lifetime_destroyed() {
    unregisterRenderer(m_rendererByLifetime[sender()]);
}

