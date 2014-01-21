#include "core_platform_abstraction.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QProcess>

#include <utils/common/warnings.h>

#include "monitoring/global_monitor.h"

#if defined(Q_OS_WIN)
#   include "process/process_win.h"
#   include "notification/notifier_win.h"
#   include "monitoring/monitor_win.h"
#   define QnProcessImpl QnWindowsProcess
#   define QnNotifierImpl QnWindowsNotifier
#   define QnMonitorImpl QnWindowsMonitor
#elif defined(Q_OS_LINUX)
#   include "process/process_unix.h"
#   include "notification/generic_notifier.h"
#   include "monitoring/monitor_linux.h"
#   define QnProcessImpl QnUnixProcess
#   define QnNotifierImpl QnGenericNotifier
#   define QnMonitorImpl QnLinuxMonitor
#elif defined(Q_OS_MACX)
#   include "process/process_unix.h"
#   include "notification/generic_notifier.h"
#   include "monitoring/monitor_mac.h"
#   define QnProcessImpl QnUnixProcess
#   define QnNotifierImpl QnGenericNotifier
#   define QnMonitorImpl QnMacMonitor
#else
#   include "process/process_unix.h"
#   include "notification/generic_notifier.h"
#   include "monitoring/sigar_monitor.h"
#   define QnProcessImpl QnUnixProcess
#   define QnNotifierImpl QnGenericNotifier
#   define QnMonitorImpl QnSigarMonitor
#endif

QnCorePlatformAbstraction::QnCorePlatformAbstraction(QObject *parent):
    QObject(parent) 
{
    if(!qApp)
        qnWarning("QApplication instance must be created before a QnCorePlatformAbstraction.");

    m_monitor = new QnGlobalMonitor(new QnMonitorImpl(this), this);
    m_notifier = new QnNotifierImpl(this);
    m_process = new QnProcessImpl(NULL, this);
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
        result = new QnProcessImpl(source, source);
        source->setProperty(qn_platformProcessPropertyName, QVariant::fromValue<QnPlatformProcess *>(result));
    }

    return result;
}
