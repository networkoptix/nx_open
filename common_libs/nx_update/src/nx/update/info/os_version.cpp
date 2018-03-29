#include <utils/common/app_info.h>
#include "os_version.h"

namespace nx {
namespace update {
namespace info {

OsVersion::OsVersion(const QString& family, const QString& architecture, const QString& version) :
    family(family),
    architecture(architecture),
    version(version)
{}

bool OsVersion::isEmpty() const
{
    return family.isEmpty() && architecture.isEmpty() && version.isEmpty();
}

bool OsVersion::matches(const QString& target) const
{
    return target.contains(family)
        && target.contains(architecture)
        && target.contains(version);
}

QString OsVersion::toString() const
{
    return lit("%1.%2.%3").arg(family).arg(architecture).arg(version);
}

OsVersion OsVersion::fromString(const QString& s)
{
    auto splits = s.split('.');
    NX_ASSERT(splits.size() == 3);
    return OsVersion(splits[0], splits[1], splits[2]);
}

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
