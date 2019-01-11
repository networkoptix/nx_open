#include "connection_url_parser.h"

#include <QtCore/QRegularExpression>

#include <utils/common/util.h>

#include <nx/network/socket_common.h>
#include <nx/network/http/http_types.h>

namespace nx::vms::client::desktop {

nx::utils::Url parseConnectionUrlFromUserInput(const QString& input)
{
    nx::utils::Url result;

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

        nx::network::SocketAddress address(match.captured("address"));
        result.setHost(address.address.toString());
        result.setPort(address.port > 0
            ? address.port
            : kDefaultConnectionPort);
    }
    else
    {
        const auto sourceUrl = nx::utils::Url::fromUserInput(input);

        result.setHost(sourceUrl.host());
        result.setPort(sourceUrl.port(kDefaultConnectionPort));
        result.setScheme(input.startsWith("http://")
            ? nx::network::http::kUrlSchemeName
            : nx::network::http::kSecureUrlSchemeName);
    }
    return result;
}

} // namespace nx::vms::client::desktop
