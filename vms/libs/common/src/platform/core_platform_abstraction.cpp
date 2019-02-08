#include "core_platform_abstraction.h"

#ifndef QT_NO_PROCESS

#include <QtCore/QCoreApplication>
#include <QtCore/QProcess>

#include <utils/common/warnings.h>

#if defined(Q_OS_WIN)
#   include "process/platform_process_win.h"
#   include "notification/notifier_win.h"
#   define QnProcessImpl QnWindowsProcess
#   define QnNotifierImpl QnWindowsNotifier
#elif defined(Q_OS_LINUX)
#   include "process/platform_process_unix.h"
#   include "notification/generic_notifier.h"
#   define QnProcessImpl QnUnixProcess
#   define QnNotifierImpl QnGenericNotifier
#elif defined(Q_OS_MACX)
#   include "process/platform_process_unix.h"
#   include "notification/generic_notifier.h"
#   define QnProcessImpl QnUnixProcess
#   define QnNotifierImpl QnGenericNotifier
#else
#   include "process/platform_process_unix.h"
#   include "notification/generic_notifier.h"
#   define QnProcessImpl QnUnixProcess
#   define QnNotifierImpl QnGenericNotifier
#endif

QnCorePlatformAbstraction::QnCorePlatformAbstraction(QObject *parent):
    QObject(parent)
{
    if(!qApp)
        qnWarning("QApplication instance must be created before a QnCorePlatformAbstraction.");

    m_notifier = new QnNotifierImpl(this);
    m_process = new QnProcessImpl(NULL, this);
}

QnCorePlatformAbstraction::~QnCorePlatformAbstraction()
{
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

#endif // QT_NO_PROCESS
