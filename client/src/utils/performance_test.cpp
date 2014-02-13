#include "performance_test.h"

#include <QtCore/QFile>
#include <QtCore/QRegExp>

#ifdef Q_OS_LINUX
namespace {
    QString getCpuName() {
        QFile cpuInfo(QLatin1String("/proc/cpuinfo"));
        if (!cpuInfo.open(QFile::ReadOnly))
            return QString();

        QString content = QLatin1String(cpuInfo.readAll());
        cpuInfo.close();

        /* Information about cores is separated by empty lines */
//        int coreNum = content.count("\n\n") + 1;

        QRegExp modelNameRegExp(QLatin1String("model name\\s+: (.*)\n"));
        modelNameRegExp.setMinimal(true);
        QString modelName;
        if (modelNameRegExp.indexIn(content) != -1)
            modelName = modelNameRegExp.cap(1);
        return modelName;
    }
}
#endif

int QnPerformanceTest::getOptimalLightMode() {
#ifdef Q_OS_LINUX
    QString cpuName = getCpuName();
#endif
    return 0;
}
