#pragma once

#include <boost/optional.hpp>

#include <QtCore/QString>

#include <nx/utils/thread/mutex.h>

namespace nx {
namespace network {
namespace cloud {

class NX_NETWORK_API CloudConnectSettings
{
public:
    /**
     * @param hostAddress will be used by mediator instead of request source address.
     * This is needed when peer is placed in the same LAN as mediator (e.g., vms_gateway).
     * In this case mediator receives sees vms_gateway under local IP address, which is useless for remote peer.
     */
    void replaceOriginatingHostAddress(const QString& hostAddress);
    boost::optional<QString> originatingHostAddressReplacement() const;

private:
    boost::optional<QString> m_originatingHostAddressReplacement;
    mutable QnMutex m_mutex;
};

} // namespace cloud
} // namespace network
} // namespace nx
