#include "update_request_data.h"

namespace nx {
namespace update {
namespace info {

static const QString kLinuxFamily = "linux";
static const QString kWindowsFamily = "windows";
static const QString kx86 = "x86";
static const QString kx64 = "x64";
static const QString kUbuntu = "ubuntu";
static const QString kWinxp = "winxp";
static const QString kArm = "arm";

OsVersion ubuntuX64()
{
    return OsVersion(kLinuxFamily, kx64, kUbuntu);
}

OsVersion ubuntuX86()
{
    return OsVersion(kLinuxFamily, kx86, kUbuntu);
}

OsVersion windowsX64()
{
    return OsVersion(kWindowsFamily, kx64, kWinxp);
}

OsVersion windowsX86()
{
    return OsVersion(kWindowsFamily, kx86, kWinxp);
}

OsVersion armBpi()
{
    return OsVersion(kLinuxFamily, kArm, "bpi");
}

OsVersion armRpi()
{
    return OsVersion(kLinuxFamily, kArm, "rpi");
}

OsVersion armBananapi()
{
    return OsVersion(kLinuxFamily, kArm, "bananapi");
}

} // namespace info
} // namespace update
} // namespace nx
