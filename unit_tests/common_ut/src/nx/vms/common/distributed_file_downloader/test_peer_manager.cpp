#include "test_peer_manager.h"

#include <nx/utils/log/assert.h>
#include <nx/vms/common/private/distributed_file_downloader/storage.h>

namespace nx {
namespace vms {
namespace common {
namespace distributed_file_downloader {

TestPeerManager::TestPeerManager()
{
}

void TestPeerManager::addPeer(const QnUuid& peerId)
{
    if (!m_peers.contains(peerId))
        m_peers.insert(peerId, PeerInfo());
}

QnUuid TestPeerManager::addPeer()
{
    const auto peerId = QnUuid::createUuid();
    addPeer(peerId);
    return peerId;
}

void TestPeerManager::setFileInformation(
    const QnUuid& peerId, const FileInformation& fileInformation)
{
    m_peers[peerId].fileInformationByName[fileInformation.name] = fileInformation;
}

TestPeerManager::FileInformation TestPeerManager::fileInformation(
    const QnUuid& peerId, const QString& fileName) const
{
    auto& peerInfo = m_peers[peerId];
    if (!peerInfo.storage)
        return m_peers[peerId].fileInformationByName[fileName];

    FileInformation fileInfo = peerInfo.storage->fileInformation(fileName);
    if (fileInfo.isValid())
        fileInfo.filePath = peerInfo.storage->filePath(fileName);

    return fileInfo;
}

void TestPeerManager::setPeerStorage(const QnUuid& peerId, Storage* storage)
{
    m_peers[peerId].storage = storage;
}

QStringList TestPeerManager::peerGroups(const QnUuid& peerId) const
{
    return m_peers[peerId].groups;
}

QList<QnUuid> TestPeerManager::peersInGroup(const QString& group) const
{
    return m_peersByGroup.values(group);
}

void TestPeerManager::setPeerGroups(const QnUuid& peerId, const QStringList& groups)
{
    m_peers[peerId].groups = groups;
    for (const auto& group: groups)
        m_peersByGroup.insert(group, peerId);
}

QnUuid TestPeerManager::selfId() const
{
    return QnUuid();
}

QList<QnUuid> TestPeerManager::getAllPeers() const
{
    return m_peers.keys();
}

int TestPeerManager::distanceTo(const QnUuid& /*peerId*/) const
{
    return std::numeric_limits<int>::max();
}

rest::Handle TestPeerManager::requestFileInfo(
    const QnUuid& peerId,
    const QString& fileName,
    AbstractPeerManager::FileInfoCallback callback)
{
    if (!m_peers.contains(peerId))
        return 0;

    const auto handle = getRequestHandle();

    enqueueCallback(
        [this, fileInfo = fileInformation(peerId, fileName), callback, handle]()
        {
            callback(fileInfo.isValid(), handle, fileInfo);
        });

    return handle;
}

rest::Handle TestPeerManager::requestChecksums(
    const QnUuid& peerId,
    const QString& fileName,
    AbstractPeerManager::ChecksumsCallback callback)
{
    auto it = m_peers.find(peerId);
    if (it == m_peers.end())
        return 0;

    const auto handle = getRequestHandle();

    enqueueCallback(
        [this,
            fileInfo = fileInformation(peerId, fileName),
            storage = it->storage,
            callback, handle]()
        {
            callback(fileInfo.isValid(), handle,
                storage
                    ? storage->getChunkChecksums(fileInfo.name)
                    : fileInfo.checksums);
        });

    return handle;
}

rest::Handle TestPeerManager::downloadChunk(
    const QnUuid& peerId,
    const QString& fileName,
    int chunkIndex,
    AbstractPeerManager::ChunkCallback callback)
{
    if (!m_peers.contains(peerId))
        return 0;

    const auto handle = getRequestHandle();

    enqueueCallback(
        [this, fileInfo = fileInformation(peerId, fileName), chunkIndex, callback, handle]()
        {
            QByteArray result;
            if (fileInfo.isValid())
                result = readFileChunk(fileInfo, chunkIndex);

            callback(!result.isNull(), handle, result);
        });

    return handle;
}

void TestPeerManager::cancelRequest(const QnUuid& /*peerId*/, rest::Handle /*handle*/)
{
    // Dummy requests are always instant, so we have nothing to do here.
}

void TestPeerManager::processRequests()
{
    QQueue<std::function<void()>> callbacksQueue;
    m_callbacksQueue.swap(callbacksQueue);

    while (!callbacksQueue.isEmpty())
    {
        auto callback = callbacksQueue.dequeue();
        callback();
    }
}

rest::Handle TestPeerManager::getRequestHandle()
{
    return ++m_requestIndex;
}

void TestPeerManager::enqueueCallback(std::function<void ()> callback)
{
    m_callbacksQueue.enqueue(callback);
}

QByteArray TestPeerManager::readFileChunk(
    const FileInformation& fileInformation, int chunkIndex)
{
    QFile file(fileInformation.filePath);
    if (!file.open(QFile::ReadOnly))
        return QByteArray();

    if (!file.seek(chunkIndex * fileInformation.chunkSize))
        return QByteArray();

    return file.read(fileInformation.chunkSize);
}

ProxyTestPeerManager::ProxyTestPeerManager(TestPeerManager* peerManager):
    ProxyTestPeerManager(peerManager, QnUuid::createUuid())
{
}

ProxyTestPeerManager::ProxyTestPeerManager(TestPeerManager* peerManager, const QnUuid& id):
    m_peerManager(peerManager),
    m_selfId(id)
{
    NX_ASSERT(peerManager);
    peerManager->addPeer(id);
}

void ProxyTestPeerManager::calculateDistances()
{
    for (const auto& peerId: m_peerManager->getAllPeers())
        m_distances[peerId] = std::numeric_limits<int>::max();

    QList<QnUuid> peersToCheck{m_selfId};
    QSet<QnUuid> checkedPeers{m_selfId};

    int distance = 0;
    while (!peersToCheck.isEmpty())
    {
        const auto peers = peersToCheck;
        peersToCheck.clear();

        for (const auto& peerId: peers)
        {
            m_distances[peerId] = distance;
            for (const auto& group: m_peerManager->peerGroups(peerId))
            {
                for (const auto& peerId: m_peerManager->peersInGroup(group))
                {
                    if (checkedPeers.contains(peerId))
                        continue;

                    peersToCheck.append(peerId);
                    checkedPeers.insert(peerId);
                }
            }
        }

        ++distance;
    }
}

QnUuid ProxyTestPeerManager::selfId() const
{
    return m_selfId;
}

QList<QnUuid> ProxyTestPeerManager::getAllPeers() const
{
    auto result = m_peerManager->getAllPeers();
    result.removeOne(m_selfId);
    return result;
}

int ProxyTestPeerManager::distanceTo(const QnUuid& peerId) const
{
    return m_distances.value(peerId, std::numeric_limits<int>::max());
}

rest::Handle ProxyTestPeerManager::requestFileInfo(
    const QnUuid& peer,
    const QString& fileName,
    AbstractPeerManager::FileInfoCallback callback)
{
    return m_peerManager->requestFileInfo(peer, fileName, callback);
}

rest::Handle ProxyTestPeerManager::requestChecksums(
    const QnUuid& peer,
    const QString& fileName,
    AbstractPeerManager::ChecksumsCallback callback)
{
    return m_peerManager->requestChecksums(peer, fileName, callback);
}

rest::Handle ProxyTestPeerManager::downloadChunk(
    const QnUuid& peer,
    const QString& fileName,
    int chunkIndex,
    AbstractPeerManager::ChunkCallback callback)
{
    return m_peerManager->downloadChunk(peer, fileName, chunkIndex, callback);
}

void ProxyTestPeerManager::cancelRequest(const QnUuid& peerId, rest::Handle handle)
{
    m_peerManager->cancelRequest(peerId, handle);
}

TestPeerManager::FileInformation::FileInformation(const DownloaderFileInformation& fileInfo):
    DownloaderFileInformation(fileInfo)
{
}

} // namespace distributed_file_downloader
} // namespace common
} // namespace vms
} // namespace nx
