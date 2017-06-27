#include "fglrx_full_screen.h"

#include <QtWidgets/QAction>
#include <QtOpenGL/QGLWidget>

#include <nx/client/desktop/ui/actions/action_manager.h>

#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/opengl/gl_functions.h>
#include <ui/workbench/workbench_display.h>

using namespace nx::client::desktop::ui;

QnFglrxFullScreen::QnFglrxFullScreen(QObject *parent):
    Instrument(Viewport, makeSet(), parent),
    QnWorkbenchContextAware(parent),
    m_fglrxMode(false)
{
    display()->instrumentManager()->installInstrument(this);

    updateFglrxMode(true);
}

void QnFglrxFullScreen::updateFglrxMode(bool force) {
    bool fglrxMode = false;

    foreach(QGraphicsView *view, display()->instrumentManager()->views()) {
        if(QGLWidget *viewport = dynamic_cast<QGLWidget *>(view->viewport())) {
            fglrxMode = (QnGlFunctions(viewport->context()).features() & QnGlFunctions::NoOpenGLFullScreen);
            if(fglrxMode)
                break;
        }
    }

    if(m_fglrxMode == fglrxMode && !force)
        return;

    m_fglrxMode = fglrxMode;

    if (m_fglrxMode)
        menu()->registerAlias(action::EffectiveMaximizeAction, action::MaximizeAction);
}

bool QnFglrxFullScreen::registeredNotify(QWidget *) {
    updateFglrxMode();
    return true;
}

void QnFglrxFullScreen::unregisteredNotify(QWidget *) {
    updateFglrxMode();
}
