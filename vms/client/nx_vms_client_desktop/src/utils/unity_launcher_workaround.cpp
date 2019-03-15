#include "unity_launcher_workaround.h"

#include <QtCore/QFileInfo>
#include <QtCore/QProcess>

#include <nx/utils/app_info.h>
#include <utils/common/process.h>

namespace nx::vms::client::desktop {
namespace utils {

bool UnityLauncherWorkaround::startDetached(const QString& program, const QStringList& arguments)
{
    if (!nx::utils::AppInfo::isLinux())
        return nx::startProcessDetached(program, arguments);

    const QFileInfo info(program);
    return QProcess::startDetached(lit("./") + info.fileName(), arguments, info.absolutePath());
}

} // namespace utils
} // namespace nx::vms::client::desktop
