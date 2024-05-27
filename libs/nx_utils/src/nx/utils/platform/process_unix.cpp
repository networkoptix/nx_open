// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "process.h"
#if !defined(QT_NO_PROCESS)

#include <sys/types.h>
#include <signal.h>

#include <QtCore/QProcess>
#include <QtCore/QFile>

namespace nx::utils {

SystemError::ErrorCode killProcessByPid(qint64 pid)
{
    return kill(pid, SIGKILL) == 0
        ? SystemError::noError
        : SystemError::getLastOSErrorCode();
}

bool startProcessDetached(
    const QString& executablePath,
    const QStringList& arguments,
    const QString& workingDirectory,
    qint64* pid)
{
    return QProcess::startDetached(executablePath, arguments, workingDirectory, pid);
}

bool checkProcessExists(qint64 pid)
{
    return kill(pid, 0) == 0;
}

} // namespace nx::utils

#endif // !defined(QT_NO_PROCESS)
