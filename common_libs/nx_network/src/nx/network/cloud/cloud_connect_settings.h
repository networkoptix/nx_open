/**********************************************************
* Jun 2, 2016
* akolesnikov
***********************************************************/

#pragma once

#include <boost/optional.hpp>

#include <QtCore/QString>


namespace nx {
namespace network {
namespace cloud {

class NX_NETWORK_API CloudConnectSettings
{
public:
    /** \a hostAddress will be used by mediator instead of request source address.
        This is needed when peer is placed in the same LAN as mediator (e.g., vms_gateway).
        In this case mediator receives sees vms_gateway under local IP address, which is useless for remote peer
    */
    void replaceOriginatingHostAddress(const QString& hostAddress);
    boost::optional<QString> originatingHostAddressReplacement() const;

private:
    boost::optional<QString> m_originatingHostAddressReplacement;
};

}   // namespace cloud
}   // namespace network
}   // namespace nx
