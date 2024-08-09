// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "client_process_runner.h"
#include "startup_parameters.h"

#include <QtCore/QCoreApplication>

#include <client/client_startup_parameters.h>
#include <utils/mac_utils.h>
#include <utils/unity_launcher_workaround.h>

#include <nx/utils/platform/process.h>

#include <nx/vms/client/desktop/ini.h>

namespace nx::vms::client::desktop {

using PidType = ClientProcessExecutionInterface::PidType;

PidType ClientProcessRunner::currentProcessPid() const
{
    return QCoreApplication::applicationPid();
}

bool ClientProcessRunner::isProcessRunning(PidType pid) const
{
    return nx::utils::checkProcessExists(pid);
}

PidType ClientProcessRunner::runClient(const QStringList& arguments) const
{
    auto execName = qApp->applicationFilePath();

    if (!QString(ini().clientExecutableName).isEmpty())
        execName = qApp->applicationDirPath() + '/' + ini().clientExecutableName;

#ifdef Q_OS_MACOS
    return mac_startDetached(execName, arguments);
#else
    return utils::UnityLauncherWorkaround::startDetached(execName, arguments);
#endif
}


PidType ClientProcessRunner::runClient(const StartupParameters& parameters) const
{
    QStringList arguments = parameters.toCommandLineParams();
    arguments << QnStartupParameters::kAllowMultipleClientInstancesKey;
    return runClient(arguments);
}

} // namespace nx::vms::client::desktop
