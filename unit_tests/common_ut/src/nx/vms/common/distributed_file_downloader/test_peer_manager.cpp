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

    auto fileInfo = it->fileInformationByName.value(fileName);

    const auto handle = getRequestHandle();

    enqueueCallback(
        [this, fileInfo, callback, handle]()
        {
            callback(fileInfo.isValid(), handle, fileInfo);
        });

    return handle;
}

rest::Handle TestPeerManager::requestChecksums(
    const QnUuid& peer,
    const QString& fileName,
    AbstractPeerManager::ChecksumsCallback callback)
{
    auto it = m_peers.find(peer);
    if (it == m_peers.end())
        return 0;

    const auto fileInfo = it->fileInformationByName.value(fileName);

    const auto handle = getRequestHandle();

    enqueueCallback(
        [this, fileInfo, callback, handle]()
        {
            callback(fileInfo.isValid(), handle, fileInfo.checksums);
        });

    return handle;
}

rest::Handle TestPeerManager::downloadChunk(
    const QnUuid& peer,
    const QString& fileName,
    int chunkIndex,
    AbstractPeerManager::ChunkCallback callback)
{
    auto it = m_peers.find(peer);
    if (it == m_peers.end())
        return 0;

    const auto fileInfo = it->fileInformationByName.value(fileName);

    const auto handle = getRequestHandle();

    enqueueCallback(
        [this, fileInfo, chunkIndex, callback, handle]()
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

QByteArray TestPeerManager::readFileChunk(const FileInformation& fileInformation, int chunkIndex)
{
    QFile file(fileInformation.filePath);
    if (!file.open(QFile::ReadOnly))
        return QByteArray();

    if (!file.seek(chunkIndex * fileInformation.chunkSize))
        return QByteArray();

    return file.read(fileInformation.chunkSize);
}

} // namespace distributed_file_downloader
} // namespace common
} // namespace vms
} // namespace nx
