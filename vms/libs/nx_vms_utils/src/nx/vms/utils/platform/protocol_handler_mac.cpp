// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "protocol_handler.h"

namespace nx::vms::utils {

using nx::utils::SoftwareVersion;

RegisterSystemUriProtocolHandlerResult registerSystemUriProtocolHandler(
    const QString& /*protocol*/,
    const QString& /*applicationBinaryPath*/,
    const QString& /*applicationName*/,
    const QString& /*description*/,
    const QString& /*customization*/,
    const SoftwareVersion& /*version*/)
{
    // Mac version registers itself through the Info.plist.
    return RegisterSystemUriProtocolHandlerResult { .success = true };
}

} // namespace nx::vms::utils
