#include "abstract_peer_manager.h"

#include <nx/utils/log/log.h>

namespace nx {
namespace vms {
namespace common {
namespace distributed_file_downloader {

AbstractPeerManager::AbstractPeerManager(QObject* parent):
    QObject(parent)
{
}

AbstractPeerManager::~AbstractPeerManager()
{
}

QString AbstractPeerManager::peerString(const QnUuid& peerId) const
{
    return peerId.toString();
}

//-------------------------------------------------------------------------------------------------

AbstractPeerManagerFactory::~AbstractPeerManagerFactory()
{
}

} // namespace distributed_file_downloader
} // namespace common
} // namespace vms
} // namespace nx
