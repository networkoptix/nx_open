#include "platform_process.h"

#include "windows_process.h"
#include "linux_process.h"

QnPlatformProcess *QnPlatformProcess::newInstance(QProcess *process, QObject *parent) {
#if defined(Q_OS_WIN)
    return new QnWindowsProcess(process, parent);
#elif defined(Q_OS_LINUX)
    return new QnLinuxProcess(process, parent);
#else
#   error Platform process is not supported on this platform.
#endif
}

