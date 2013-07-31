#include "gl_hardware_checker.h"

#include <QtGui/QMessageBox>

#include <utils/common/log.h>
#include <utils/common/warnings.h>

#include <ui/graphics/opengl/gl_functions.h>


bool QnGlHardwareChecker::checkCurrentContext(bool displayWarnings) {
    const QGLContext* context = QGLContext::currentContext();

    QnGlFunctions functions(context);

    QByteArray extensions = reinterpret_cast<const char *>(glGetString(GL_EXTENSIONS));
    QByteArray version = reinterpret_cast<const char *>(glGetString(GL_VERSION));
    QByteArray renderer = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
    QByteArray vendor = reinterpret_cast<const char *>(glGetString(GL_VENDOR));

    cl_log.log(QString(QLatin1String("OpenGL extensions: %1.")).arg(QLatin1String(extensions.constData())), cl_logINFO);
    cl_log.log(QString(QLatin1String("OpenGL version: %1.")).arg(QLatin1String(version.constData())), cl_logINFO);
    cl_log.log(QString(QLatin1String("OpenGL renderer: %1.")).arg(QLatin1String(renderer.constData())), cl_logINFO);
    cl_log.log(QString(QLatin1String("OpenGL vendor: %1.")).arg(QLatin1String(vendor.constData())), cl_logINFO);

    bool softwareTrouble = false;
    bool hardwareTrouble = false;

    if (!(functions.features() & QnGlFunctions::OpenGL2_0)) {
        qnWarning("OpenGL version %1 is not supported.", version);
        softwareTrouble = true;
    }

    /* Note that message will be shown in destructor, 
     * close to the event loop. */
    if(displayWarnings) {
        if (hardwareTrouble) {
            const QString message = tr("We have detected that your video card is not supported. You can proceed at your own risk.<br/>"
                "Installing and/or updating your video drivers may resolve the problem but we cannot guarantee that it will help.<br/>"
                "For easy instructions on how to install or update your video driver, follow instruction at <a href=\"http://tribaltrouble.com/driversupport.php\">http://tribaltrouble.com/driversupport.php</a>");
            QMessageBox::critical(NULL, tr("Critical Performance Tip"), message, QMessageBox::Ok);
        } else if(softwareTrouble) {
            const QString message = tr("We have detected that your video card drivers may be not installed or are out of date.<br/>"
                "Installing and/or updating your video drivers can substantially increase your system performance when viewing and working with video.<br/>"
                "For easy instructions on how to install or update your video driver, follow instruction at <a href=\"http://tribaltrouble.com/driversupport.php\">http://tribaltrouble.com/driversupport.php</a>");
            QMessageBox::critical(NULL, tr("Important Performance Tip"), message, QMessageBox::Ok);
        }
    }

    return !hardwareTrouble && !softwareTrouble;
}
