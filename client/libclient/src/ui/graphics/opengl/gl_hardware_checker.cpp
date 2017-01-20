#include "gl_hardware_checker.h"

#include <nx/utils/log/log.h>
#include <utils/common/warnings.h>
#include <utils/common/software_version.h>
#include <utils/common/app_info.h>

bool QnGlHardwareChecker::checkCurrentContext(bool displayWarnings) {
    QByteArray extensionsString = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));
    QByteArray versionString = reinterpret_cast<const char *>(glGetString(GL_VERSION));
    QByteArray rendererString = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
    QByteArray vendorString = reinterpret_cast<const char *>(glGetString(GL_VENDOR));

    cl_log.log(lit("OpenGL extensions: %1.").arg(QLatin1String(extensionsString.constData())), cl_logINFO);
    cl_log.log(lit("OpenGL version: %1.").arg(QLatin1String(versionString.constData())), cl_logINFO);
    cl_log.log(lit("OpenGL renderer: %1.").arg(QLatin1String(rendererString.constData())), cl_logINFO);
    cl_log.log(lit("OpenGL vendor: %1.").arg(QLatin1String(vendorString.constData())), cl_logINFO);

    bool contextIsValid = true;

    if (!versionString.contains("ES 2.0")) { //TODO: #asinaisky more strict check required
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
        QnMessageBox::_warning(nullptr,
            tr("Video card drivers are outdated or not installed"),
            tr("%1 Client may not work properly.").arg(QnAppInfo::productNameLong()));
    }

    return contextIsValid;
}
