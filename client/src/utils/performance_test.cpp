#include "performance_test.h"

#include <GL/gl.h>

#include <QtCore/QFile>
#include <QtCore/QRegExp>

#include <utils/common/performance.h>
#include <client/client_globals.h>

int QnPerformanceTest::getOptimalLightMode() {
#ifdef Q_OS_LINUX
    bool poorCpu = false;
    bool poorGpu = false;

    QString cpuName = QnPerformance::cpuName();
    QRegExp atomCpuRegExp("Intel\\(R\\) Atom\\(TM\\) CPU .*");
    if (atomCpuRegExp.exactMatch(cpuName))
        poorCpu = true;

    QString renderer = glGetString(GL_RENDERER);
    QRegExp slowRendererRegExp("Gallium .* on llvmpipe .*");
    if (slowRendererRegExp.exactMatch(renderer))
        poorGpu = true;

    if (poorCpu && poorGpu)
        return Qn::LightModeFull;
    else
        return 0;
#endif
    return 0;
}
