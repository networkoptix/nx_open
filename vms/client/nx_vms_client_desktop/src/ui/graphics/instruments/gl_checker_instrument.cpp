#include "gl_checker_instrument.h"

#include <QtGui/QOpenGLContext>
#include <QtGui/QOpenGLFunctions>
#include <QtWidgets/QAction>
#include <QtWidgets/QOpenGLWidget>

#include <nx/utils/log/log.h>
#include <nx/utils/software_version.h>
#include <ui/workbench/workbench_display.h>
#include <ui/graphics/opengl/gl_functions.h>
#include <ui/graphics/instruments/instrument_manager.h>
#include <ui/dialogs/common/message_box.h>

#include <nx/vms/client/desktop/ui/actions/action_manager.h>

#include <client/client_app_info.h>

using namespace nx::vms::client::desktop::ui;

namespace {

/**
 * This class implements a workaround for ATI fglrx drivers that do not perform
 * well in fullscreen OpenGL. For these drivers, this class replaces fullscreen
 * action with a maximize action.
 */
class FglrxFullScreenWorkaround
{
public:
    FglrxFullScreenWorkaround(action::Manager* manager):
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
            m_manager->registerAlias(action::EffectiveMaximizeAction, action::MaximizeAction);
    }

private:
    action::Manager* const m_manager;
    bool m_fglrxMode = false;
};

//--------------------------------------------------------------------------------------------------

QByteArray getString(QOpenGLFunctions* functions, GLint id)
{
    return reinterpret_cast<const char*>(functions->glGetString(id));
}

void checkGLHardware(QOpenGLWidget* viewport)
{
    const auto context = viewport->context();
    if (!context)
    {
        NX_ASSERT(false, "No current OpenGL context!");
        return;
    }

    const auto functions = context->functions();

    const auto extensionsString = getString(functions, GL_EXTENSIONS);
    const QByteArray versionString = getString(functions, GL_VERSION);
    const QByteArray rendererString = getString(functions, GL_RENDERER);
    const QByteArray vendorString = getString(functions, GL_VENDOR);

    NX_INFO(NX_SCOPE_TAG, "Version: %1.", versionString);
    NX_INFO(NX_SCOPE_TAG, "Renderer: %1.", rendererString);
    NX_INFO(NX_SCOPE_TAG, "Vendor: %1.", vendorString);
    NX_DEBUG(NX_SCOPE_TAG, "Extensions: %1.", extensionsString);

    bool contextIsValid = true;

    if (!versionString.contains("ES 2.0"))
    {
        if (nx::utils::SoftwareVersion(versionString) < nx::utils::SoftwareVersion(2, 0))
        {
            NX_ERROR(NX_SCOPE_TAG, "OpenGL version %1 is not supported.", versionString);
            contextIsValid = false;
        }
    }

    /* Note that message will be shown in destructor,
     * close to the event loop. */
    if (contextIsValid)
        return;

    QnMessageBox::warning(nullptr,
        QnGLCheckerInstrument::tr("Video card drivers are outdated or not installed"),
        QnGLCheckerInstrument::tr("%1 may not work properly.").arg(QnClientAppInfo::applicationDisplayName()));
}

} // namespace

//--------------------------------------------------------------------------------------------------

struct QnGLCheckerInstrument::Private
{
    Private(action::Manager* manager):
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

    checkGLHardware(viewport);
    d->fglrxWorkaround.update(viewport);

    return result;
}

