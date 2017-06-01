#pragma once

#include <nx/utils/system_error.h>

namespace nx {

    /** Kills process by calling TerminateProcess on WIndows and sending SIGKILL on UNIX. */
    SystemError::ErrorCode killProcessByPid(qint64 pid);

    /** Start process and detach from it.
        On Linux this function uses \a posix_spawn, since \a fork is too heavy
        (especially on ARM11 platform).
        @param executablePath Path to executable binary or shell script.
        @param arguments Program arguments.
        @param workingDirectory If not empty, working directory is set to this path.
        @param pid if not \a nullptr, launched process pid is stored here.
        @return Error code.
    */
    SystemError::ErrorCode startProcessDetached(
        const QString& executablePath,
        const QStringList& arguments,
        const QString& workingDirectory = QString(),
        qint64* pid = nullptr);

    bool checkProcessExists(qint64 pid);

} // namespace nx
