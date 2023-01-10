// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "unity_launcher_workaround.h"

#include <QtCore/QFileInfo>
#include <QtCore/QProcess>

#include <nx/utils/platform/process.h>

#include <nx/build_info.h>

namespace nx::vms::client::desktop {
namespace utils {

bool UnityLauncherWorkaround::startDetached(
    const QString& program,
    const QStringList& arguments,
    bool detachOutput)
{
    if (!nx::build_info::isLinux())
        return nx::utils::startProcessDetached(program, arguments);

    const QFileInfo info(program);
    QProcess process;
    process.setProgram(QString("./") + info.fileName());
    process.setArguments(arguments);
    process.setWorkingDirectory(info.absolutePath());
    if (detachOutput)
    {
        process.setStandardOutputFile(QProcess::nullDevice());
        process.setStandardErrorFile(QProcess::nullDevice());
    }
    return process.startDetached();
}

} // namespace utils
} // namespace nx::vms::client::desktop
