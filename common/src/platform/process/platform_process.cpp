#include "platform_process.h"

#include "process_win.h"
#include "process_unix.h"

QnPlatformProcess *QnPlatformProcess::newInstance(QProcess *process, QObject *parent) {
#if defined(Q_OS_WIN)
    return new QnWindowsProcess(process, parent);
#else
//#elif defined(Q_OS_LINUX)
    return new QnLinuxProcess(process, parent);
//#else
//#   error Platform process is not supported on this platform.
#endif
}

