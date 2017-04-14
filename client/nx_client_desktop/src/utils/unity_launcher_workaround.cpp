#include "unity_launcher_workaround.h"

#include <QtCore/QFileInfo>
#include <QtCore/QProcess>

#include <utils/common/app_info.h>

namespace nx {
namespace client {
namespace desktop {
namespace utils {

bool UnityLauncherWorkaround::startDetached(const QString& program, const QStringList& arguments)
{
    if (QnAppInfo::applicationPlatform() != lit("linux"))
        return QProcess::startDetached(program, arguments);

    const QFileInfo info(program);
    return QProcess::startDetached(lit("./") + info.fileName(), arguments, info.absolutePath());
}

} // namespace utils
} // namespace desktop
} // namespace client
} // namespace nx
