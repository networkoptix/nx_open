#include "process.h"
#if !defined(QT_NO_PROCESS)

#include <sys/types.h>
#include <signal.h>

#include <QtCore/QProcess>
#include <QtCore/QFile>

#if defined(ISD_JAGUAR)
    #include <spawn.h>
    // ISD toolchain provides a newer version of posix_spawn which cannot be found on the device.
    __asm__(".symver posix_spawn,posix_spawn@GLIBC_2.4");
#endif

namespace nx {

SystemError::ErrorCode killProcessByPid(qint64 pid)
{
    return kill(pid, SIGKILL) == 0
        ? SystemError::noError
        : SystemError::getLastOSErrorCode();
}

SystemError::ErrorCode startProcessDetached(
    const QString& executablePath,
    const QStringList& arguments,
    const QString& workingDirectory,
    qint64* pid)
{
    // TODO: #ak on linux should always use posix_spawn.

    #if defined(ISD_JAGUAR)
        // TODO: #ak support workingDirectory.
        // TODO: #ak check if executablePath is script or binary.

        static const char* kShellExecutable = "/bin/sh";
        const int kExtraArgs = 3; //< Shell executable, target executable and nullptr.

        char** argv = new char*[arguments.size() + kExtraArgs];
        int argIndex = 0;

        argv[argIndex] = new char[sizeof(kShellExecutable)];
        strcpy(argv[argIndex], kShellExecutable);
        ++argIndex;

        const auto executable = QFile::encodeName(executablePath);
        argv[argIndex] = new char[executable.size() + 1];
        strcpy(argv[argIndex], executable.constData());
        ++argIndex;

        for (const auto& arg: arguments)
        {
            const auto encodedArg = arg.toLatin1();
            argv[argIndex] = new char[encodedArg.size() + 1];
            strcpy(argv[argIndex], encodedArg.constData());
            ++argIndex;
        }
        argv[argIndex] = nullptr;

        pid_t childPid = -1;
        if (posix_spawn(
            &childPid, executable.constData(), nullptr, nullptr, argv, nullptr) != 0)
        {
            return SystemError::getLastOSErrorCode();
        }

        if (pid)
            *pid = childPid;

        return SystemError::noError;
    #else
        return QProcess::startDetached(executablePath, arguments, workingDirectory, pid)
            ? SystemError::noError
            : SystemError::fileNotFound; // TODO: #ak get proper error code.
    #endif
}

bool checkProcessExists(qint64 pid)
{
    return kill(pid, 0) == 0;
}

} // namespace nx

#endif // !defined(QT_NO_PROCESS)
