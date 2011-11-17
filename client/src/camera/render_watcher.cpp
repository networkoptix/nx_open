#include "render_watcher.h"
#include <utils/common/warnings.h>


QnRenderWatcher::QnRenderWatcher(QObject *parent): 
    QObject(parent)
{}

void QnRenderWatcher::registerRenderer(CLAbstractRenderer *renderer, QObject *lifetime) {
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

    m_infoByRenderer[renderer] = Info(0, true, lifetime);
    m_rendererByLifetime[lifetime] = renderer;

    connect(lifetime, SIGNAL(destroyed()), this, SLOT(at_lifetime_destroyed()));
}

void QnRenderWatcher::unregisterRenderer(CLAbstractRenderer *renderer) {
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

void QnRenderWatcher::at_lifetime_destroyed() {
    unregisterRenderer(m_rendererByLifetime[sender()]);
}

void QnRenderWatcher::startDisplay() {
    for(QHash<CLAbstractRenderer *, Info>::iterator pos = m_infoByRenderer.begin(); pos != m_infoByRenderer.end(); pos++)
        pos->displayCounter = pos.key()->displayCounter();
}

void QnRenderWatcher::finishDisplay() {
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
