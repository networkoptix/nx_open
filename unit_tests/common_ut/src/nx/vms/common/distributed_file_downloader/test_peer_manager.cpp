#include "test_peer_manager.h"

namespace nx {
namespace vms {
namespace common {
namespace distributed_file_downloader {

TestPeerManager::TestPeerManager()
{
}

void TestPeerManager::addPeer(const QnUuid& peer)
{
    if (!m_peers.contains(peer))
        m_peers.insert(peer, PeerInfo());
}

QnUuid TestPeerManager::addPeer()
{
    const auto peerId = QnUuid::createUuid();
    addPeer(peerId);
    return peerId;
}

void TestPeerManager::setFileInformation(
    const QnUuid& peer, const FileInformation& fileInformation)
{
    m_peers[peer].fileInformationByName[fileInformation.name] = fileInformation;
}

TestPeerManager::FileInformation TestPeerManager::fileInformation(
    const QnUuid& peer, const QString& fileName) const
{
    return m_peers[peer].fileInformationByName[fileName];
}

QList<QnUuid> TestPeerManager::getAllPeers() const
{
    return m_peers.keys();
}

rest::Handle TestPeerManager::requestFileInfo(
    const QnUuid& peer,
    const QString& fileName,
    AbstractPeerManager::FileInfoCallback callback)
{
    auto it = m_peers.find(peer);
    if (it == m_peers.end())
        return 0;

    ++m_requestIndex;

    DownloaderFileInformation fileInfo = it->fileInformationByName.value(fileName);
    if (fileInfo.isValid())
    {
        callback(true, m_requestIndex, fileInfo);
    }
    else
    {
        fileInfo.status = DownloaderFileInformation::Status::notFound;
        callback(false, m_requestIndex, fileInfo);
    }

    return m_requestIndex;
}

void TestPeerManager::cancelRequest(const QnUuid& /*peerId*/, rest::Handle /*handle*/)
{
    // Dummy requests are always instant, so we have nothing to do here.
}

} // namespace distributed_file_downloader
} // namespace common
} // namespace vms
} // namespace nx
