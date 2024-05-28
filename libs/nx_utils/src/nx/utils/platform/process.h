// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>
#include <QtCore/QStringList>

#include <nx/utils/system_error.h>

class QProcess;

namespace nx::utils {

/**
 * Kill process by calling TerminateProcess on Windows and sending SIGKILL on UNIX.
 */
NX_UTILS_API SystemError::ErrorCode killProcessByPid(qint64 pid);

/**
 * Start process and detach from it.
 * On Linux this function uses posix_spawn, since fork is too heavy
 * (especially on ARM11 platform).
 * @param executablePath Path to executable binary or shell script.
 * @param arguments Program arguments.
 * @param workingDirectory If not empty, working directory is set to this path.
 * @param pid if not null, launched process pid is stored here.
 * @return Whether process was started succesfully.
 */
NX_UTILS_API bool startProcessDetached(
    const QString& executablePath,
    const QStringList& arguments = QStringList(),
    const QString& workingDirectory = QString(),
    qint64* pid = nullptr);

/**
 * Start process.
 * QProcess has several overload functions QProcess::start().
 * Some of them aren't working on Windows.This wrapper helps to avoid this.
 * @param executablePath Path to executable binary or shell script.
 * @param arguments Program arguments.
 * @param workingDirectory If not empty, working directory is set to this path.
 * @return Unique pointer to QProcess that has been already started.
 */
NX_UTILS_API std::unique_ptr<QProcess> startProcess(
    const QString& executablePath,
    const QStringList& arguments = QStringList(),
    const QString& workingDirectory = QString());


NX_UTILS_API bool checkProcessExists(qint64 pid);

} // namespace nx::utils
