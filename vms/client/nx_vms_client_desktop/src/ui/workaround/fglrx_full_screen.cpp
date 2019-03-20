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

void QnFglrxFullScreen::updateFglrxMode(QOpenGLWidget* viewport, bool force) {
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
    viewport->installEventFilter(this);
    return true;
}

void QnFglrxFullScreen::unregisteredNotify(QWidget* viewport)
{
    viewport->removeEventFilter(this);
}

bool QnFglrxFullScreen::eventFilter(QObject* watched, QEvent* event)
{
    const bool result = base_type::eventFilter(watched, event);
    const auto viewport = qobject_cast<QOpenGLWidget*>(watched);
    if (!viewport || event->type() != QEvent::UpdateRequest)
        return result;

    viewport->removeEventFilter(this);
    updateFglrxMode(viewport);
    return result;
}
