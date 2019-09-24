#include <nx/cloud/relay/libtraffic_relay_app_info.h>

namespace nx {
namespace cloud {
namespace relay {

QString AppInfo::applicationName()
{
    return QStringLiteral("${customization.companyName} libtraffic_relay");
}

QString AppInfo::applicationDisplayName()
{
    return QStringLiteral("${customization.companyName} Traffic Relay");
}

} // namespace relay
} // namespace cloud
} // namespace nx
