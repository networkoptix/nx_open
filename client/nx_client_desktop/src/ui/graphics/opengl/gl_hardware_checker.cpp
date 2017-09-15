#include "gl_hardware_checker.h"

#include <QtGui/QOpenGLFunctions>

#include <client/client_app_info.h>

#include <utils/common/warnings.h>
#include <utils/common/software_version.h>

#include <nx/utils/log/log.h>


bool QnGlHardwareChecker::checkCurrentContext(bool displayWarnings)
{
    QByteArray extensionsString = reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS));
    QByteArray versionString = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    QByteArray rendererString = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    QByteArray vendorString = reinterpret_cast<const char*>(glGetString(GL_VENDOR));

    static const nx::utils::log::Tag kLogTag(typeid(QnGlHardwareChecker));
    NX_INFO(kLogTag, lit("Extensions: %1.").arg(QLatin1String(extensionsString.constData())));
    NX_INFO(kLogTag, lit("Version: %1.").arg(QLatin1String(versionString.constData())));
    NX_INFO(kLogTag, lit("Renderer: %1.").arg(QLatin1String(rendererString.constData())));
    NX_INFO(kLogTag, lit("Vendor: %1.").arg(QLatin1String(vendorString.constData())));

    bool contextIsValid = true;

    if (!versionString.contains("ES 2.0")) { // TODO: #asinaisky more strict check required
        QnSoftwareVersion version(versionString);
        if (version < QnSoftwareVersion(2, 0, 0, 0)) {
            qnWarning("OpenGL version %1 is not supported.", versionString);
            contextIsValid = false;
        }
    }

    /* Note that message will be shown in destructor,
     * close to the event loop. */
    if(displayWarnings && !contextIsValid)
    {
        QnMessageBox::warning(nullptr,
            tr("Video card drivers are outdated or not installed"),
            tr("%1 may not work properly.").arg(QnClientAppInfo::applicationDisplayName()));
    }

    return contextIsValid;
}
