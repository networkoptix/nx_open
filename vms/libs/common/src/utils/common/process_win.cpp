#include "process.h"

#include <windows.h>

#include <QtCore/QProcess>

namespace nx {

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

SystemError::ErrorCode startProcessDetached(
    const QString& executablePath,
    const QStringList& arguments,
    const QString& workingDirectory,
    qint64* pid)
{
    QProcess p;
    p.setProgram(executablePath);
    p.setWorkingDirectory(workingDirectory);
    p.setArguments(arguments);
    p.setCreateProcessArgumentsModifier(
        [](QProcess::CreateProcessArguments* args)
        {
            args->startupInfo->dwFlags &= ~DWORD(STARTF_USESTDHANDLES);
            args->flags |= CREATE_NEW_CONSOLE;
        });

    return p.startDetached(pid)? SystemError::noError : SystemError::fileNotFound;
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

} // namespace nx
