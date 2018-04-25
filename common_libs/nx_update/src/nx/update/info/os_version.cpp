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

QString OsVersion::serialize() const
{
    return lit("%1.%2.%3").arg(family).arg(architecture).arg(version);
}

OsVersion OsVersion::deserialize(const QString& s)
{
    auto splits = s.split('.');
    NX_ASSERT(splits.size() == 3);
    return OsVersion(splits[0], splits[1], splits[2]);
}

OsVersion OsVersion::fromString(const QString& s)
{
    if (s == "bananapi")
        return armBananapi();
    else if (s == "rpi")
        return armRpi();
    else if (s == "bpi")
        return armBpi();
    else if (s == "linux86")
        return ubuntuX86();
    else if (s == "linux64")
        return ubuntuX64();
    else if (s == "win64")
        return windowsX64();
    else if (s == "win86")
        return windowsX86();
    else if (s == "mac")
        return macosx();

    NX_ASSERT(false, "Unknown OS string");
    return OsVersion();
}

bool operator==(const OsVersion& lhs, const OsVersion& rhs)
{
    return lhs.architecture == rhs.architecture && lhs.family == rhs.family
        && lhs.version == rhs.version;
}

static const QString kLinuxFamily = "linux";
static const QString kWindowsFamily = "windows";
static const QString kMacOsxFamily = "macosx";
static const QString kMacOsx = kMacOsxFamily;
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

OsVersion macosx()
{
    return OsVersion(kMacOsxFamily, kx64, kMacOsx);
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
