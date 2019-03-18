#include "fglrx_full_screen.h"

#include <QtWidgets/QAction>
#include <QtWidgets/QOpenGLWidget>

#include <nx/vms/client/desktop/ui/actions/action_manager.h>

#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/opengl/gl_functions.h>
#include <ui/workbench/workbench_display.h>

using namespace nx::vms::client::desktop::ui;

QnFglrxFullScreen::QnFglrxFullScreen(QObject *parent):
    Instrument(Viewport, makeSet(), parent),
    QnWorkbenchContextAware(parent),
    m_fglrxMode(false)
{
    display()->instrumentManager()->installInstrument(this);

    updateFglrxMode(nullptr, true);
}

void QnFglrxFullScreen::updateFglrxMode(QWidget* viewport, bool force) {
    bool fglrxMode = false;

    if(QOpenGLWidget* glWidget = dynamic_cast<QOpenGLWidget*>(viewport))
        fglrxMode = QnGlFunctions(glWidget).features().testFlag(QnGlFunctions::NoOpenGLFullScreen);

    if(m_fglrxMode == fglrxMode && !force)
        return;

    m_fglrxMode = fglrxMode;

    if (m_fglrxMode)
        menu()->registerAlias(action::EffectiveMaximizeAction, action::MaximizeAction);
}

bool QnFglrxFullScreen::registeredNotify(QWidget* viewport)
{
    //updateFglrxMode(viewport);
    return true;
}

void QnFglrxFullScreen::unregisteredNotify(QWidget* viewport)
{
    //updateFglrxMode(viewport);
}
