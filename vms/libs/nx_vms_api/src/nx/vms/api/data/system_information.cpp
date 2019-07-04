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

SystemInformation::SystemInformation(
    const QString& platform,
    const QString& arch,
    const QString& modification,
    const QString& version)
    :
    arch(arch),
    platform(platform),
    modification(modification)
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

} // namespace api
} // namespace vms
} // namespace nx

#if defined(Q_OS_OSX)

#include <sys/types.h>
#include <sys/sysctl.h>

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

QString nx::vms::api::SystemInformation::currentSystemRuntime()
{
    return "Unknown";
}

#endif // defined(Q_OS_OSX)
