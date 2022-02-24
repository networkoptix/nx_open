// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "connection_url_parser.h"

#include <QtCore/QRegularExpression>

#include <utils/common/util.h>
#include <network/system_helpers.h>

#include <nx/network/socket_common.h>
#include <nx/network/http/http_types.h>

namespace nx::vms::client::desktop {

nx::utils::Url parseConnectionUrlFromUserInput(const QString& input)
{
    nx::utils::Url result;

    if (input.isEmpty())
        return result;

    // Trying to parse assuming we have a correct network url here.
    static const QRegularExpression re(
        "^((?<protocol>.*)://)?(.*@)?(?<address>.*?)([/?&#].*)?$");
    const auto match = re.match(input);
    if (match.hasMatch())
    {
        const auto protocol = match.captured("protocol");
        result.setScheme(protocol == nx::network::http::kUrlSchemeName
            ? nx::network::http::kUrlSchemeName
            : nx::network::http::kSecureUrlSchemeName);

        nx::network::SocketAddress address(match.captured("address").toStdString());
        result.setHost(address.address.toString());
        result.setPort(address.port > 0
            ? address.port
            : helpers::kDefaultConnectionPort);
    }
    else
    {
        const auto sourceUrl = nx::utils::Url::fromUserInput(input);

        result.setHost(sourceUrl.host());
        result.setPort(sourceUrl.port(helpers::kDefaultConnectionPort));
        result.setScheme(input.startsWith("http://")
            ? nx::network::http::kUrlSchemeName
            : nx::network::http::kSecureUrlSchemeName);
    }
    return result;
}

} // namespace nx::vms::client::desktop
