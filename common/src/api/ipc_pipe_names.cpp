#include "ipc_pipe_names.h"

#include <nx/utils/app_info.h>

QString launcherPipeName()
{
    const auto baseName = nx::utils::AppInfo::customizationName()
        + lit("EC4C367A-FEF0-4fa9-B33D-DF5B0C767788");

    return nx::utils::AppInfo::isMacOsX()
        ? baseName + QString::fromLatin1(qgetenv("USER").toBase64())
        : baseName;
}


