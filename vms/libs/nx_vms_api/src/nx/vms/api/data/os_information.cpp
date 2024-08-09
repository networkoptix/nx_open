// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "os_information.h"

#include <QtCore/QRegularExpression>

#include <nx/fusion/model_functions.h>
#include <nx/utils/log/assert.h>

#include <nx/build_info.h>

namespace nx {
namespace vms {
namespace api {

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    OsInformation, (hash)(ubjson)(json)(xml)(csv_record), OsInformation_Fields)

OsInformation::OsInformation(
    const QString& platform,
    const QString& arch,
    const QString& modification)
    :
    arch(arch),
    platform(platform),
    modification(modification)
{
}

OsInformation::OsInformation(const QString& infoString)
{
    const QRegularExpression infoRegExp(
        QRegularExpression::anchoredPattern(QLatin1String("(\\S+)\\s+(\\S+)\\s*(\\S*)")));

    if (const auto match = infoRegExp.match(infoString); match.hasMatch())
    {
        platform = match.captured(1);
        arch = match.captured(2);
        modification = match.captured(3);
    }
}

QString OsInformation::toString() const
{
    const auto result = QStringLiteral("%1 %2").arg(platform, arch);
    return modification.isEmpty()
        ? result
        : QStringLiteral("%1 %2").arg(result, modification);
}

bool OsInformation::isValid() const
{
    return !platform.isEmpty() && !arch.isEmpty();
}

OsInformation OsInformation::fromBuildInfo()
{
    static const OsInformation kCurrentSystemInformation{
        nx::build_info::applicationPlatform(),
        nx::build_info::applicationArch(),
        nx::build_info::applicationPlatformModification()};
    return kCurrentSystemInformation;
}

} // namespace api
} // namespace vms
} // namespace nx

#if defined(Q_OS_MACOS)

#include <sys/types.h>
#include <sys/sysctl.h>

QString nx::vms::api::OsInformation::currentSystemRuntime()
{
    char osrelease[256];
    int mibMem[2] = {CTL_KERN, KERN_OSRELEASE};
    size_t size = sizeof(osrelease);
    if (sysctl(mibMem, 2, osrelease, &size, NULL, 0) != -1)
        return QString::fromStdString(osrelease);

    return "OSX without KERN_OSRELEASE";
}

#elif !defined(Q_OS_LINUX) && !defined(Q_OS_WIN)

QString nx::vms::api::OsInformation::currentSystemRuntime()
{
    return "Unknown";
}

#endif // defined(Q_OS_MACOS)
