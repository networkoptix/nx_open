#include "system_information.h"

#include <QtCore/QString>

namespace nx::vms::api {

QString SystemInformation::currentSystemRuntime()
{
    return "iOS";
}

QString SystemInformation::runtimeOsVersion()
{
    return QString();
}

} // namespace nx::vms::api
