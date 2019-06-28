#include "system_information.h"

#include <QtCore/QSysInfo>

namespace nx::vms::api {

QString SystemInformation::currentSystemRuntime()
{
    return QSysInfo::prettyProductName();
}

} // namespace nx::vms::api
