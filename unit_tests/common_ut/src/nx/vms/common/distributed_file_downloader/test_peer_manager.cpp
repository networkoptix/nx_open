#include "test_peer_manager.h"

#include <nx/utils/log/assert.h>
#include <nx/vms/common/distributed_file_downloader/private/storage.h>

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

void TestPeerManager::setHasInternetConnection(const QnUuid& peerId, bool hasInternetConnection)
{
    m_peers[peerId].hasInternetConnection = hasInternetConnection;
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

void TestPeerManager::addInternetFile(const QUrl& url, const QString& fileName)
{
    m_fileByUrl[url] = fileName;
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

bool TestPeerManager::hasInternetConnection(const QnUuid& peerId) const
{
    return m_peers[peerId].hasInternetConnection;
}

rest::Handle TestPeerManager::requestFileInfo(
    const QnUuid& peerId,
    const QString& fileName,
    AbstractPeerManager::FileInfoCallback callback)
{
    if (!m_peers.contains(peerId))
        return 0;

    return enqueueRequest(peerId,
        [this, fileInfo = fileInformation(peerId, fileName), callback](rest::Handle handle)
        {
            callback(fileInfo.isValid(), handle, fileInfo);
        });
}

rest::Handle TestPeerManager::requestChecksums(
    const QnUuid& peerId,
    const QString& fileName,
    AbstractPeerManager::ChecksumsCallback callback)
{
    auto it = m_peers.find(peerId);
    if (it == m_peers.end())
        return 0;

    return enqueueRequest(peerId,
        [this,
            fileInfo = fileInformation(peerId, fileName),
            storage = it->storage,
            callback]
            (rest::Handle handle)
        {
            callback(fileInfo.isValid(), handle,
                storage
                    ? storage->getChunkChecksums(fileInfo.name)
                    : fileInfo.checksums);
        });
}

rest::Handle TestPeerManager::downloadChunk(
    const QnUuid& peerId,
    const QString& fileName,
    int chunkIndex,
    AbstractPeerManager::ChunkCallback callback)
{
    if (!m_peers.contains(peerId))
        return 0;

    return enqueueRequest(peerId,
        [this,
            fileInfo = fileInformation(peerId, fileName),
            chunkIndex,
            callback]
            (rest::Handle handle)
        {
            QByteArray result;
            if (fileInfo.isValid())
                result = readFileChunk(fileInfo, chunkIndex);

            callback(!result.isNull(), handle, result);
        });
}

rest::Handle TestPeerManager::downloadChunkFromInternet(
    const QnUuid& peerId,
    const QUrl& fileUrl,
    int chunkIndex,
    int chunkSize,
    AbstractPeerManager::ChunkCallback callback)
{
    if (!m_peers.contains(peerId))
        return 0;

    return enqueueRequest(peerId,
        [this, peerId, fileUrl, chunkIndex, chunkSize, callback](rest::Handle handle)
        {
            if (!hasInternetConnection(peerId))
            {
                callback(false, handle, QByteArray());
                return;
            }

            const auto fileName = m_fileByUrl.value(fileUrl);
            if (fileName.isEmpty())
            {
                callback(false, handle, QByteArray());
                return;
            }

            FileInformation fileInfo;
            fileInfo.filePath = fileName;
            fileInfo.chunkSize = chunkSize;

            auto result = readFileChunk(fileInfo, chunkIndex);
            callback(!result.isNull(), handle, result);
        });
}

void TestPeerManager::cancelRequest(const QnUuid& peerId, rest::Handle handle)
{
    auto it = std::remove_if(m_requestsQueue.begin(), m_requestsQueue.end(),
        [&peerId, &handle](const Request& request)
        {
            return request.peerId == peerId && request.handle == handle;
        });
    m_requestsQueue.erase(it, m_requestsQueue.end());
}

void TestPeerManager::processRequests()
{
    QQueue<Request> callbacksQueue;
    m_requestsQueue.swap(callbacksQueue);

    while (!callbacksQueue.isEmpty())
    {
        const auto& request = callbacksQueue.dequeue();
        request.callback(request.handle);
    }
}

rest::Handle TestPeerManager::getRequestHandle()
{
    return ++m_requestIndex;
}

rest::Handle TestPeerManager::enqueueRequest(const QnUuid& peerId, RequestCallback callback)
{
    const auto handle = getRequestHandle();
    m_requestsQueue.enqueue(Request{peerId, handle, callback});
    return handle;
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

bool ProxyTestPeerManager::hasInternetConnection(const QnUuid& peerId) const
{
    return m_peerManager->hasInternetConnection(peerId);
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

rest::Handle ProxyTestPeerManager::downloadChunkFromInternet(
    const QnUuid& peerId,
    const QUrl& fileUrl,
    int chunkIndex,
    int chunkSize,
    AbstractPeerManager::ChunkCallback callback)
{
    return m_peerManager->downloadChunkFromInternet(
        peerId, fileUrl, chunkIndex, chunkSize, callback);
}

void ProxyTestPeerManager::cancelRequest(const QnUuid& peerId, rest::Handle handle)
{
    m_peerManager->cancelRequest(peerId, handle);
}

TestPeerManager::FileInformation::FileInformation(
    const distributed_file_downloader::FileInformation& fileInfo)
    :
    distributed_file_downloader::FileInformation(fileInfo)
{
}

} // namespace distributed_file_downloader
} // namespace common
} // namespace vms
} // namespace nx
