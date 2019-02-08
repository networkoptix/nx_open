#include "protocol_handler.h"

namespace nx::vms::utils {

using nx::utils::SoftwareVersion;

bool registerSystemUriProtocolHandler(
    const QString& /*protocol*/,
    const QString& /*applicationBinaryPath*/,
    const QString& /*applicationName*/,
    const QString& /*description*/,
    const QString& /*customization*/,
    const SoftwareVersion& /*version*/)
{
    // Mac version registers itself through the Info.plist.
    return true;
}

} // namespace nx::vms::utils
