#include "system_information.h"

#include <QtCore/QRegExp>

#include <nx/fusion/model_functions.h>
#include <nx/utils/log/assert.h>

namespace nx {
namespace vms {
namespace api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (SystemInformation),
    (eq)(hash)(ubjson)(json)(xml)(csv_record)(datastream),
    _Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (SystemInformationNew),
    (eq)(hash)(ubjson)(json),
    _Fields)

QString SystemInformation::runtimeOsVersionOverride;
QString SystemInformation::runtimeModificationOverride;

SystemInformation::SystemInformation(
    const QString& platform,
    const QString& arch,
    const QString& modification,
    const QString& version)
    :
    arch(arch),
    platform(platform),
    modification(modification),
    version(version)
{
}

SystemInformation::SystemInformation(const QString& infoString)
{
    QRegExp infoRegExp(QLatin1String("(\\S+)\\s+(\\S+)\\s*(\\S*)"));
    if (infoRegExp.exactMatch(infoString))
    {
        platform = infoRegExp.cap(1);
        arch = infoRegExp.cap(2);
        modification = infoRegExp.cap(3);
    }
}

QString SystemInformation::toString() const
{
    const auto result = QStringLiteral("%1 %2").arg(platform, arch);
    return modification.isEmpty()
        ? result
        : QStringLiteral("%1 %2").arg(result, modification);
}

bool SystemInformation::isValid() const
{
    return !platform.isEmpty() && !arch.isEmpty();
}

SystemInformationNew SystemInformationNew::fromLegacySystemInformation(
    const SystemInformation& info)
{
    if (info.modification == "bpi")
        return {"nx1", {}, info.version};
    if (info.modification == "edge1" || info.modification == "nx1")
        return {info.modification, {}, info.version};
    if (info.platform == "ios")
        return {info.platform, {}, info.version};
    if (info.platform == "macosx")
        return {"macos", {}, info.version};

    const QString arch = info.arch == "arm" ? "arm32" : info.arch;
    const QString platform = info.platform + L'_' + arch;

    static QSet<QString> kObsoleteModifications{"winxp", "rpi", "bananapi"};
    const QString modification =
        kObsoleteModifications.contains(info.modification) ? QString() : info.modification;

    return {platform, modification, info.version};

}

SystemInformation SystemInformationNew::toLegacySystemInformation() const
{
    SystemInformation info;
    info.modification = variant;
    info.version = variantVersion;

    const QStringList& components = platform.split(L'_');
    if (components.size() == 2)
    {
        info.platform = components.first();
        info.arch = components.last();
        if (info.arch == "arm32")
            info.arch = "arm";
        return info;
    }

    if (platform == "nx1" || platform == "bpi")
        return SystemInformation("linux", "arm", "bpi", variantVersion);
    if (platform == "edge1")
        return SystemInformation("linux", "arm", platform, variantVersion);
    if (platform == "ios")
        return SystemInformation("ios", "arm", "none", variantVersion);
    if (info.platform == "macos")
        return SystemInformation("macosx", "x64", "none", variantVersion);

    NX_ASSERT(false, "Could not convert SystemInformation");

    return {};
}

QString toString(const SystemInformationNew& systemInformation)
{
    return QStringList{
        systemInformation.platform,
        systemInformation.variant,
        systemInformation.variantVersion}.join(L'-');
}

} // namespace api
} // namespace vms
} // namespace nx

#if defined(Q_OS_OSX)

#include <sys/types.h>
#include <sys/sysctl.h>

QString nx::vms::api::SystemInformation::runtimeModification()
{
    return runtimeModificationOverride;
}

QString nx::vms::api::SystemInformation::runtimeOsVersion()
{
    // #TODO #dkargin implement
    return runtimeOsVersionOverride;
}

QString nx::vms::api::SystemInformation::currentSystemRuntime()
{
    char osrelease[256];
    int mibMem[2] = {CTL_KERN, KERN_OSRELEASE};
    size_t size = sizeof(osrelease);
    if (sysctl(mibMem, 2, osrelease, &size, NULL, 0) != -1)
        return QString::fromStdString(osrelease);

    return "OSX without KERN_OSRELEASE";
}

#elif !defined(Q_OS_LINUX) && !defined(Q_OS_WIN)

QString nx::vms::api::SystemInformation::runtimeModification()
{
    return runtimeModificationOverride;
}

QString nx::vms::api::SystemInformation::runtimeOsVersion()
{
    // #TODO #dkargin implement
    return runtimeOsVersionOverride;
}

QString nx::vms::api::SystemInformation::currentSystemRuntime()
{
    return "Unknown";
}

#endif // defined(Q_OS_OSX)
