// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <QtCore/QString>

#include <nx/utils/url.h>

namespace nx::vms::client::desktop {

/**
 * Parses string as if it was entered manually by user and interprets it as a server connection
 * url. Default port is substituted if needed.
 */
NX_VMS_CLIENT_DESKTOP_API nx::utils::Url parseConnectionUrlFromUserInput(const QString& input);

} // namespace nx::vms::client::desktop
