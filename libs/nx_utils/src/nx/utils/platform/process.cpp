// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "process.h"

#include <QtCore/QProcess>

namespace nx::utils {

#if !defined(Q_OS_IOS)

std::unique_ptr<QProcess> startProcess(
    const QString& executablePath,
    const QStringList& arguments,
    const QString& workingDirectory)
{
    auto process = std::make_unique<QProcess>();
    process->setProgram(executablePath);
    process->setArguments(arguments);
    if (!workingDirectory.isNull())
        process->setWorkingDirectory(workingDirectory);
    process->start();
    return process;
}

#endif

} // namespace nx::utils
