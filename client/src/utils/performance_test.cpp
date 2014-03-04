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

void QnPerformanceTest::detectLightMode() {
    if (qnSettings->lightModeOverride() != -1) {
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

    {
        // It creates OpenGL context
        QWidget hideWidget;
        QGLWidget *openGlDummy = new QGLWidget(&hideWidget);
        openGlDummy->show();

        QString renderer = QLatin1String(reinterpret_cast<const char *>(glGetString(GL_RENDERER)));
        QRegExp slowRendererRegExp(lit("Gallium .* on llvmpipe .*"));
        if (slowRendererRegExp.exactMatch(renderer))
            poorGpu = true;
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
