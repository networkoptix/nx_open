#pragma once

#include <QtCore/QString>

#include <nx/utils/url.h>

namespace nx::vms::client::desktop {
namespace helpers {

/**
 * Parses string as if it was entered manually by user and interprets it as a server connection
 * url. Default port is substituted if needed.
 */
NX_VMS_CLIENT_DESKTOP_API nx::utils::Url parseConnectionUrlFromUserInput(const QString& input);

} // namespace helpers
} // namespace nx::vms::client::desktop
