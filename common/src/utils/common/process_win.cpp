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
    return QProcess::startDetached(executablePath, arguments, workingDirectory, pid)
        ? SystemError::noError
        : SystemError::fileNotFound; //TODO: #ak get proper error code.
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
