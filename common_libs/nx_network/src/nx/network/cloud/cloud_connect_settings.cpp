/**********************************************************
* Jun 2, 2016
* akolesnikov
***********************************************************/

#include "cloud_connect_settings.h"


namespace nx {
namespace network {
namespace cloud {

void CloudConnectSettings::replaceOriginatingHostAddress(const QString& hostAddress)
{
    m_originatingHostAddressReplacement = hostAddress;
}

boost::optional<QString> CloudConnectSettings::originatingHostAddressReplacement() const
{
    return m_originatingHostAddressReplacement;
}

}   // namespace cloud
}   // namespace network
}   // namespace nx
