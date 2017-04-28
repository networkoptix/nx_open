#include "abstract_peer_manager.h"

#include <nx/utils/log/log.h>

namespace {

constexpr int kDefaultPeersPerOperation = 5;

} // namespace

namespace nx {
namespace vms {
namespace common {
namespace distributed_file_downloader {

AbstractPeerManager::AbstractPeerManager(QObject* parent):
    QObject(parent),
    m_peersPerOperation(kDefaultPeersPerOperation)
{
}

AbstractPeerManager::~AbstractPeerManager()
{
}

int AbstractPeerManager::peersPerOperation() const
{
    return m_peersPerOperation;
}

void AbstractPeerManager::setPeersPerOperation(int peersPerOperation)
{
    m_peersPerOperation = peersPerOperation;
}

QList<QnUuid> AbstractPeerManager::selectPeersForOperation()
{
    auto peers = getAllPeers();

    if (peers.size() <= m_peersPerOperation)
        return peers;

    QList<QnUuid> result;

    // TODO: #dklychkov Implement intelligent server selection.
    for (int i = 0; i < m_peersPerOperation; ++i)
        result.append(peers.takeAt(rand() % peers.size()));

    return result;
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
