// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "gl_checker_instrument.h"

#include <QtGui/QAction>
#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtOpenGLWidgets/QOpenGLWidget>

#include <nx/branding.h>
#include <nx/utils/log/log.h>
#include <nx/utils/software_version.h>
#include <nx/vms/client/desktop/menu/action_manager.h>
#include <ui/dialogs/common/message_box.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/graphics/opengl/gl_functions.h>
#include <ui/workbench/workbench_display.h>

using namespace nx::vms::client::desktop;

namespace {

/**
 * This class implements a workaround for ATI fglrx drivers that do not perform
 * well in fullscreen OpenGL. For these drivers, this class replaces fullscreen
 * action with a maximize action.
 */
class FglrxFullScreenWorkaround
{
public:
    FglrxFullScreenWorkaround(menu::Manager* manager):
        m_manager(manager)
    {
    }

    void update(QOpenGLWidget* viewport)
    {
        bool fglrxMode = false;

        if(QOpenGLWidget* glWidget = dynamic_cast<QOpenGLWidget*>(viewport))
            fglrxMode = QnGlFunctions(glWidget).features().testFlag(QnGlFunctions::NoOpenGLFullScreen);

        if(m_fglrxMode == fglrxMode)
            return;

        m_fglrxMode = fglrxMode;

        if (m_fglrxMode)
            m_manager->registerAlias(menu::EffectiveMaximizeAction, menu::MaximizeAction);
    }

private:
    menu::Manager* const m_manager;
    bool m_fglrxMode = false;
};

} // namespace

//--------------------------------------------------------------------------------------------------

struct QnGLCheckerInstrument::Private
{
    Private(menu::Manager* manager):
        fglrxWorkaround(manager)
    {
    }

    FglrxFullScreenWorkaround fglrxWorkaround;
};

//--------------------------------------------------------------------------------------------------

QnGLCheckerInstrument::QnGLCheckerInstrument(QObject* parent):
    base_type(Viewport, makeSet(), parent),
    QnWorkbenchContextAware(parent),
    d(new Private(menu()))
{
    display()->instrumentManager()->installInstrument(this);
}

QnGLCheckerInstrument::~QnGLCheckerInstrument()
{
}

void QnGLCheckerInstrument::checkGLHardware()
{
    const auto info = QnGlFunctions::openGLInfo();

    NX_INFO(NX_SCOPE_TAG, "Version: %1.", info.version);
    NX_INFO(NX_SCOPE_TAG, "Renderer: %1.", info.renderer);
    NX_INFO(NX_SCOPE_TAG, "Vendor: %1.", info.vendor);
    NX_DEBUG(NX_SCOPE_TAG, "Extensions: %1.", info.extensions);

    bool contextIsValid = true;

    if (!info.version.contains("ES 2.0"))
    {
        if (nx::utils::SoftwareVersion(info.version) < nx::utils::SoftwareVersion(2, 0))
        {
            NX_ERROR(NX_SCOPE_TAG, "OpenGL version %1 is not supported.", info.version);
            contextIsValid = false;
        }
    }

    // Note that message will be shown in destructor, close to the event loop.
    if (contextIsValid)
        return;

    QnMessageBox::warning(nullptr,
        tr("Video card drivers are outdated or not installed"),
        tr("%1 may not work properly.")
            .arg(nx::branding::desktopClientDisplayName()));
}

bool QnGLCheckerInstrument::registeredNotify(QWidget *viewport)
{
    viewport->installEventFilter(this);
    return true;
}

void QnGLCheckerInstrument::unregisteredNotify(QWidget *viewport)
{
    viewport->removeEventFilter(this);
}

bool QnGLCheckerInstrument::eventFilter(QObject* watched, QEvent* event)
{
    const bool result = base_type::eventFilter(watched, event);
    const auto viewport = qobject_cast<QOpenGLWidget*>(watched);
    if (!viewport || event->type() != QEvent::Paint)
        return result;

    viewport->removeEventFilter(this);
    d->fglrxWorkaround.update(viewport);

    return result;
}
