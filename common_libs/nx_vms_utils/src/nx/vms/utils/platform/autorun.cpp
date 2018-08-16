#include "autorun.h"

#include <QtCore/QDir>
#include <QtCore/QSettings>

#include <nx/utils/app_info.h>
#include <nx/utils/log/assert.h>

namespace {

const QString kWindowsRegistryPath(
    "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run");

} // namespace

namespace nx {
namespace vms {
namespace utils {

bool isAutoRunSupported()
{
    return nx::utils::AppInfo::isWindows();
}

QString autoRunPath(const QString& key)
{
    if (!isAutoRunSupported())
        return QString();

    NX_ASSERT(nx::utils::AppInfo::isWindows());
    QSettings settings(kWindowsRegistryPath, QSettings::NativeFormat);
    return settings.value(key).toString();
}

bool isAutoRunEnabled(const QString& key)
{
    return !autoRunPath(key).isEmpty();
}

void setAutoRunEnabled(const QString& key, const QString& path, bool value)
{
    if (!isAutoRunSupported())
        return;

    NX_ASSERT(nx::utils::AppInfo::isWindows());
    QSettings settings(kWindowsRegistryPath, QSettings::NativeFormat);
    if (value)
        settings.setValue(key, path);
    else
        settings.remove(key);
}

} // namespace utils
} // namespace vms
} // namespace nx
