#include "abstract_peer_manager.h"

#include <nx/utils/log/log.h>

namespace nx {
namespace vms {
namespace common {
namespace p2p {
namespace downloader {

AbstractPeerManager::AbstractPeerManager(QObject* parent):
    QObject(parent)
{
}

AbstractPeerManager::~AbstractPeerManager()
{
}

QString AbstractPeerManager::peerString(const QnUuid& peerId) const
{
    auto result = peerId.toString();
    if (peerId == selfId())
        result += lit(" (self)");
    return result;
}

//-------------------------------------------------------------------------------------------------

AbstractPeerManagerFactory::~AbstractPeerManagerFactory()
{
}

} // namespace downloader
} // namespace p2p
} // namespace common
} // namespace vms
} // namespace nx
