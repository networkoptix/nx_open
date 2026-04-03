// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "process.h"

#include <windows.h>

#include <QtCore/QProcess>

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
    auto processHandle = OpenProcess(PROCESS_TERMINATE, /*inheritHandle*/ FALSE, pid);
    if (!processHandle)
        return SystemError::getLastOSErrorCode();
    const auto result = TerminateProcess(processHandle, /*exitCode*/ 1)
        ? SystemError::noError
        : SystemError::getLastOSErrorCode();
    CloseHandle(processHandle);
    return result;
}

bool startProcessDetached(
    const QString& executablePath,
    const QStringList& arguments,
    const QString& workingDirectory,
    qint64* pid)
{
    auto p = createProcess(executablePath, arguments, workingDirectory);
    p->setCreateProcessArgumentsModifier(
        [](QProcess::CreateProcessArguments* args)
        {
            args->startupInfo->dwFlags &= ~DWORD(STARTF_USESTDHANDLES);
            args->flags |= CREATE_NEW_CONSOLE;
            args->inheritHandles = false;
        });

    return p->startDetached(pid);
}

bool checkProcessExists(qint64 pid)
{
    bool exists = false;

    auto handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, /*inheritHandle*/ FALSE, pid);
    if (handle)
    {
        DWORD exitCode = 9999;
        if (GetExitCodeProcess(handle, &exitCode))
            exists = exitCode == STILL_ACTIVE;
        CloseHandle(handle);
    }

    return exists;
}

std::unique_ptr<QProcess> startProcess(
    const QString& executablePath,
    const QStringList& arguments,
    const QString& workingDirectory)
{
    auto p = createProcess(executablePath, arguments, workingDirectory);
    p->setCreateProcessArgumentsModifier(
        [](QProcess::CreateProcessArguments* args)
        {
            args->inheritHandles = false;
        });
    p->start();
    return p;
}

} // namespace nx::utils
