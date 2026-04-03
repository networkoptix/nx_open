// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "process.h"
#if !defined(QT_NO_PROCESS)

#include <sys/types.h>
#include <signal.h>

#include <QtCore/QProcess>
#include <QtCore/QFile>

namespace {

std::unique_ptr<QProcess> createProcess(
    const QString& executablePath,
    const QStringList& arguments,
    const QString& workingDirectory)
{
    auto process = std::make_unique<QProcess>();
    process->setProgram(executablePath);
    process->setArguments(arguments);
    if (!workingDirectory.isNull())
        process->setWorkingDirectory(workingDirectory);
    return process;
}

} // anonymous namespace

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
    auto p = createProcess(executablePath, arguments, workingDirectory);
    return p->startDetached(pid);
}

bool checkProcessExists(qint64 pid)
{
    return kill(pid, 0) == 0;
}

std::unique_ptr<QProcess> startProcess(
    const QString& executablePath,
    const QStringList& arguments,
    const QString& workingDirectory)
{
    auto p = createProcess(executablePath, arguments, workingDirectory);
    p->start();
    return p;
}

} // namespace nx::utils

#endif // !defined(QT_NO_PROCESS)
