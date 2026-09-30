// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "process_utils.h"

#include <nx/utils/platform/process.h>

#if defined(Q_OS_MACOS)
    #include <libproc.h>

    #include <vector>

    #include <QtCore/QFileInfo>
#endif

#ifdef Q_OS_LINUX
    #include <signal.h>
    #include <unistd.h>

    #include <QtCore/QFile>

    #include <sys/wait.h>

void sig_child_handler(int signum)
{
    signal(signum, &sig_child_handler);
    wait(nullptr);
}

bool ProcessUtils::startProcessDetached(const QString& program,
    const QStringList& arguments,
    const QString& workingDirectory,
    const QStringList& environment)
{
    /* prepare argv */
    QList<QByteArray> enc_args;
    enc_args.append(QFile::encodeName(program));
    for (int i = 0; i < arguments.size(); ++i)
        enc_args.append(arguments.at(i).toLocal8Bit());

    const int argc = enc_args.size();
    QScopedArrayPointer<char*> raw_argv(new char*[argc + 1]);
    for (int i = 0; i < argc; ++i)
        raw_argv[i] = const_cast<char *>(enc_args.at(i).data());
    raw_argv[argc] = 0;

    /* prepare environment */
    QList<QByteArray> enc_env;
    foreach (const QString &s, environment)
        enc_env.append(s.toLocal8Bit());

    int envc = enc_env.size();
    QScopedArrayPointer<char*> raw_env(new char*[envc + 1]);
    for (int i = 0; i < envc; ++i)
        raw_env[i] = const_cast<char *>(enc_env.at(i).data());
    raw_env[envc] = 0;

    // Encode the working directory if it's non-empty, otherwise just pass 0.
    QByteArray encodedName;
    if (!workingDirectory.isEmpty())
        encodedName = QFile::encodeName(workingDirectory);

    int fileDescriptorsLimit = static_cast<int>(sysconf(_SC_OPEN_MAX));

    pid_t childPid = fork();

    if (childPid == 0)
    {
        // Close all file descriptors except for stdin, stdout, and stderr.
        for (int i = STDERR_FILENO + 1; i < fileDescriptorsLimit; i++)
            close(i);

        if (!encodedName.isEmpty())
            chdir(encodedName.constData());

        execve(enc_args[0], raw_argv.data(), raw_env.data());
        ::exit(-1);
    }

    return childPid != -1;
}

void ProcessUtils::initialize()
{
    signal(SIGCHLD, &sig_child_handler);
}

#else

bool ProcessUtils::startProcessDetached(const QString& program,
    const QStringList& arguments,
    const QString& workingDirectory,
    const QStringList& environment)
{
    if(program.isEmpty())
        return false; /* This prevents a crash in QProcess::startDetached. */

    return nx::utils::startProcessDetached(program, arguments, workingDirectory);
}

void ProcessUtils::initialize() {}

#endif

#if defined(Q_OS_MACOS)

std::optional<bool> ProcessUtils::isProcessRunning(const QString& executableName)
{
    const auto requiredSizeBytes = proc_listpids(PROC_ALL_PIDS, 0, nullptr, 0);
    if (requiredSizeBytes <= 0)
        return std::nullopt;

    static constexpr auto kPidsHeadroom = 64; //< For processes started after the size was queried.
    std::vector<pid_t> pids(requiredSizeBytes / sizeof(pid_t) + kPidsHeadroom);

    const auto filledSizeBytes = proc_listpids(
        PROC_ALL_PIDS, 0, pids.data(), static_cast<int>(pids.size() * sizeof(pid_t)));
    if (filledSizeBytes <= 0)
        return std::nullopt;

    pids.resize(filledSizeBytes / sizeof(pid_t));

    char path[PROC_PIDPATHINFO_MAXSIZE];
    for (const auto pid: pids)
    {
        if (pid <= 0 || proc_pidpath(pid, path, sizeof(path)) <= 0)
            continue;

        if (QFileInfo(QString::fromUtf8(path)).fileName() == executableName)
            return true;
    }
    return false;
}

#endif // defined(Q_OS_MACOS)
