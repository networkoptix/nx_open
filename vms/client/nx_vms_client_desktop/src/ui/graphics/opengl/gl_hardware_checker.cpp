#include "gl_hardware_checker.h"

#include <QtGui/QOpenGLFunctions>

#include <nx/utils/log/log.h>
#include <nx/utils/literal.h>

#include <utils/common/warnings.h>
#include <nx/utils/software_version.h>

#include <client/client_app_info.h>
#include <ui/dialogs/common/message_box.h>

bool QnGlHardwareChecker::checkCurrentContext(bool displayWarnings)
{
    return true;
    const QByteArray extensionsString = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
    const QByteArray versionString = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const QByteArray rendererString = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const QByteArray vendorString = reinterpret_cast<const char*>(glGetString(GL_VENDOR));

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
    if (displayWarnings && !contextIsValid)
    {
        QnMessageBox::warning(nullptr,
            tr("Video card drivers are outdated or not installed"),
            tr("%1 may not work properly.").arg(QnClientAppInfo::applicationDisplayName()));
    }

    return contextIsValid;
}
