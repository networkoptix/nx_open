#include "performance_test.h"

#include <QtCore/QFile>
#include <QtCore/QRegExp>
#include <QtCore/QCoreApplication>
#include <QtOpenGL/QGLWidget>

#include <utils/common/app_info.h>
#include <utils/common/performance.h>
#include <client/client_globals.h>
#include <client/client_settings.h>
#include <ui/dialogs/message_box.h>

#ifdef Q_OS_LINUX
#include <X11/X.h>
#include <X11/Xlib.h>
#include <GL/glx.h>
#include <QtX11Extras/QX11Info>
#endif

void QnPerformanceTest::detectLightMode() {
    if (qnSettings->lightModeOverride() != 0) {
        qnSettings->setLightMode(qnSettings->lightModeOverride());
        return;
    }

    bool poorCpu = false;
    bool poorGpu = false;

#ifdef Q_OS_LINUX
    QString cpuName = QnPerformance::cpuName();
    QRegExp poorCpuRegExp(lit("Intel\\(R\\) (Atom\\(TM\\)|Celeron\\(R\\)) CPU .*"));
    poorCpu = poorCpuRegExp.exactMatch(cpuName);

    // Create OpenGL context and check GL_RENDERER
    if (Display *display = QX11Info::display()) {
        GLint attr[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
        if (XVisualInfo *visual = glXChooseVisual(display, 0, attr)) {
            if (GLXContext context = glXCreateContext(display, visual, NULL, GL_TRUE)) {
                glXMakeCurrent(display, DefaultRootWindow(display), context);

                QString renderer = QLatin1String(reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
                QRegExp poorRendererRegExp(lit("Gallium .* on llvmpipe .*|Mesa DRI Intel\\(R\\) Bay Trail"));
                poorGpu = poorRendererRegExp.exactMatch(renderer);

                glXDestroyContext(display, context);
            }
            XFree(visual);
        }
    }
#endif

    if (poorCpu && poorGpu) {
         qnSettings->setLightMode(Qn::LightModeFull);
        // TODO: #dklychkov change context in translate()
        QnMessageBox::warning(
            NULL,
            0,
            QCoreApplication::translate("QnPerformance", "Warning"),
            QCoreApplication::translate("QnPerformance", "Performance of this computer allows running %1 in configuration mode only. "
                                                         "For full-featured mode please use another computer.").
                    arg(QnAppInfo::productNameLong()),
            QMessageBox::StandardButtons(QMessageBox::Ok),
            QMessageBox::Ok
        );
    }
}
