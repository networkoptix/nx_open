#include "performance_test.h"

#include <QtCore/QFile>
#include <QtCore/QRegExp>
#include <QtCore/QCoreApplication>
#include <QtOpenGL/QGLWidget>

#include <version.h>
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
    QRegExp atomCpuRegExp(lit("Intel\\(R\\) Atom\\(TM\\) CPU .*"));
    if (atomCpuRegExp.exactMatch(cpuName))
        poorCpu = true;

    // Create OpenGL context and check GL_RENDERER
    if (Display *display = QX11Info::display()) {
        GLint attr[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
        if (XVisualInfo *visual = glXChooseVisual(display, 0, attr)) {
            if (GLXContext context = glXCreateContext(display, visual, NULL, GL_TRUE)) {
                glXMakeCurrent(display, DefaultRootWindow(display), context);

                QString renderer = QLatin1String(reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
                QRegExp slowRendererRegExp(lit("Gallium .* on llvmpipe .*"));
                if (slowRendererRegExp.exactMatch(renderer))
                    poorGpu = true;

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
                    arg(QLatin1String(QN_PRODUCT_NAME_LONG)),
            QMessageBox::StandardButtons(QMessageBox::Ok),
            QMessageBox::Ok
        );
    }
}
