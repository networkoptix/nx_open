#include "core_platform_abstraction.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QProcess>

#include <utils/common/warnings.h>

#if defined(Q_OS_WIN)
#   include "process/process_win.h"
#else
#   include "process/process_unix.h"
#endif

namespace {
    QnPlatformProcess *newPlatformProcess(QProcess *process, QObject *parent) {
#if defined(Q_OS_WIN)
        return new QnWindowsProcess(process, parent);
#else
        return new QnUnixProcess(process, parent);
#endif
    }

} // anonymous namespace


QnCorePlatformAbstraction::QnCorePlatformAbstraction(QObject *parent):
    QObject(parent) 
{
    if(!qApp)
        qnWarning("QApplication instance must be created before a QnCorePlatformAbstraction.");

    m_monitor = new QnGlobalMonitor(QnPlatformMonitor::newInstance(this), this);
    m_notifier = QnPlatformNotifier::newInstance(this);
    m_process = newPlatformProcess(NULL, this);
}

QnCorePlatformAbstraction::~QnCorePlatformAbstraction() {
    return;
}

QnPlatformProcess *QnCorePlatformAbstraction::process(QProcess *source) const {
    if(source == NULL)
        return m_process;

    static const char *qn_platformProcessPropertyName = "_qn_platformProcess";
    QnPlatformProcess *result = source->property(qn_platformProcessPropertyName).value<QnPlatformProcess *>();
    if(!result) {
        result = newPlatformProcess(source, source);
        source->setProperty(qn_platformProcessPropertyName, QVariant::fromValue<QnPlatformProcess *>(result));
    }

    return result;
}
