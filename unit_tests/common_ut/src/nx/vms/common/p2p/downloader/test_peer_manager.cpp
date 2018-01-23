#include "test_peer_manager.h"

#include <QtCore/QTextStream>

#include <nx/utils/log/assert.h>
#include <nx/vms/common/p2p/downloader/private/storage.h>

namespace nx {
namespace vms {
namespace common {
namespace p2p {
namespace downloader {

namespace {

static const auto kNullGuid = QnUuid();
static constexpr int kDefaultRequestTime = 200;

} // namespace

TestPeerManager::TestPeerManager(TestPeerManagerHandler* handler):
    m_handler(handler)
{
}

void TestPeerManager::addPeer(const QnUuid& peerId, const QString& peerName)
{
    if (!m_peers.contains(peerId))
    {
        PeerInfo info;
        info.name = peerName;
        m_peers.insert(peerId, info);
    }
}

QnUuid TestPeerManager::addPeer(const QString& peerName)
{
    const auto peerId = QnUuid::createUuid();
    addPeer(peerId, peerName);
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

void TestPeerManager::addInternetFile(const nx::utils::Url& url, const QString& fileName)
{
    m_fileByUrl[url] = fileName;
}

QnUuid TestPeerManager::selfId() const
{
    return QnUuid();
}

QString TestPeerManager::peerString(const QnUuid& peerId) const
{
    auto result = m_peers[peerId].name;
    if (result.isEmpty())
        result = peerId.toString();

    if (peerId == selfId())
        result += " (self)";

    return result;
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
    m_requestCounter.incrementCounters(peerId, RequestCounter::FileInfoRequest);

    if (!m_peers.contains(peerId))
        return 0;

    return enqueueRequest(peerId, kDefaultRequestTime,
        [this, fileInfo = fileInformation(peerId, fileName), callback](rest::Handle handle)
        {
            callback(fileInfo.isValid(), handle, fileInfo);
            m_handler->onRequestFileInfo();
        });
}

rest::Handle TestPeerManager::requestChecksums(
    const QnUuid& peerId,
    const QString& fileName,
    AbstractPeerManager::ChecksumsCallback callback)
{
    m_requestCounter.incrementCounters(peerId, RequestCounter::ChecksumsRequest);

    auto it = m_peers.find(peerId);
    if (it == m_peers.end())
        return 0;

    return enqueueRequest(peerId, kDefaultRequestTime,
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
    m_requestCounter.incrementCounters(peerId, RequestCounter::DownloadChunkRequest);

    if (!m_peers.contains(peerId))
        return 0;

    return enqueueRequest(peerId, kDefaultRequestTime,
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

rest::Handle TestPeerManager::downloadChunkFromInternet(const QnUuid& peerId,
    const QString& fileName,
    const utils::Url &url,
    int chunkIndex,
    int chunkSize,
    AbstractPeerManager::ChunkCallback callback)
{
    m_requestCounter.incrementCounters(peerId, RequestCounter::DownloadChunkFromInternetRequest);

    if (!m_peers.contains(peerId))
        return 0;

    return enqueueRequest(peerId, kDefaultRequestTime,
        [this, peerId, fileName, url, chunkIndex, chunkSize, callback](rest::Handle handle)
        {
            Storage* storage = nullptr;
            downloader::FileInformation storageFileInfo;

            auto it = m_peers.find(peerId);
            if (it != m_peers.end() && it->storage)
            {
                storageFileInfo = it->storage->fileInformation(fileName);
                if (storageFileInfo.isValid()
                    && storageFileInfo.url == url
                    && storageFileInfo.chunkSize == chunkSize
                    && chunkIndex < storageFileInfo.downloadedChunks.size())
                {
                    storage = it->storage;
                }
            }

            if (storage && storageFileInfo.downloadedChunks[chunkIndex])
            {
                QByteArray result;
                if (storage->readFileChunk(fileName, chunkIndex, result) == ResultCode::ok)
                {
                    callback(true, handle, result);
                    return;
                }
            }

            if (!hasInternetConnection(peerId))
            {
                callback(false, handle, QByteArray());
                return;
            }

            const auto filePath = m_fileByUrl.value(url);
            if (filePath.isEmpty())
            {
                callback(false, handle, QByteArray());
                return;
            }

            FileInformation fileInfo;
            fileInfo.filePath = filePath;
            fileInfo.chunkSize = chunkSize;

            auto result = readFileChunk(fileInfo, chunkIndex);
            m_requestCounter.incrementCounters(peerId,
                RequestCounter::InternetDownloadRequestsPerformed);

            if (storage)
                storage->writeFileChunk(fileName, chunkIndex, result);

            callback(!result.isNull(), handle, result);
        });
}

rest::Handle TestPeerManager::validateFileInformation(
    const downloader::FileInformation& fileInformation,
    AbstractPeerManager::ValidateCallback callback)
{
    return enqueueRequest(selfId(), kDefaultRequestTime,
        [this, callback](rest::Handle handle)
        {
            callback(true, handle);
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

bool TestPeerManager::processNextRequest()
{
    if (m_requestsQueue.isEmpty())
        return false;

    const auto& request = m_requestsQueue.dequeue();
    m_currentTime = request.timeToReply;
    request.callback(request.handle);

    return true;
}

void TestPeerManager::exec(int maxRequests)
{
    while (processNextRequest())
    {
        if (maxRequests > 1)
            --maxRequests;
        else if (maxRequests == 1)
            break;
    }
}

const RequestCounter* TestPeerManager::requestCounter() const
{
    return &m_requestCounter;
}

void TestPeerManager::requestWait(const QnUuid& peerId, qint64 waitTime, WaitCallback callback)
{
    if (!m_peers.contains(peerId))
        return;

    enqueueRequest(peerId, waitTime, [this, callback](rest::Handle) { callback(); });
}

rest::Handle TestPeerManager::getRequestHandle()
{
    return ++m_requestIndex;
}

rest::Handle TestPeerManager::enqueueRequest(
    const QnUuid& peerId, qint64 time, RequestCallback callback)
{
    const auto handle = getRequestHandle();
    Request request{peerId, handle, callback, m_currentTime + time};

    //auto it = std::lower_bound(m_requestsQueue.begin(), m_requestsQueue.end(), request,
    //    [](const Request& left, const Request& right)
    //    {
    //        return left.timeToReply < right.timeToReply;
    //    });

    QnMutexLocker lock(&m_mutex);
    //m_requestsQueue.insert(it, request);
    m_requestsQueue.push_back(request);
    m_condition.wakeOne();
    return handle;
}

void TestPeerManager::pleaseStop()
{
    QnMutexLocker lock(&m_mutex);
    m_needStop = true;
    m_condition.wakeOne();
}

void TestPeerManager::run()
{
    QnMutexLocker lock(&m_mutex);
    while (!needToStop())
    {
        while (m_requestsQueue.isEmpty() && !needToStop())
            m_condition.wait(lock.mutex());

        if (needToStop())
            return;

        const auto request = m_requestsQueue.front();
        m_requestsQueue.pop_front();

        lock.unlock();
        request.callback(request.handle);
        lock.relock();
    }
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

//-------------------------------------------------------------------------------------------------

ProxyTestPeerManager::ProxyTestPeerManager(TestPeerManager* peerManager, const QString& peerName):
    ProxyTestPeerManager(peerManager, QnUuid::createUuid(), peerName)
{
}

ProxyTestPeerManager::ProxyTestPeerManager(
    TestPeerManager* peerManager, const QnUuid& id, const QString& peerName)
    :
    m_peerManager(peerManager),
    m_selfId(id)
{
    NX_ASSERT(peerManager);
    peerManager->addPeer(id, peerName);
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

const RequestCounter* ProxyTestPeerManager::requestCounter() const
{
    return &m_requestCounter;
}

void ProxyTestPeerManager::requestWait(qint64 waitTime, TestPeerManager::WaitCallback callback)
{
    m_peerManager->requestWait(m_selfId, waitTime, callback);
}

QnUuid ProxyTestPeerManager::selfId() const
{
    return m_selfId;
}

QString ProxyTestPeerManager::peerString(const QnUuid& peerId) const
{
    return m_peerManager->peerString(peerId);
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
    m_requestCounter.incrementCounters(peer, RequestCounter::FileInfoRequest);
    return m_peerManager->requestFileInfo(peer, fileName, callback);
}

rest::Handle ProxyTestPeerManager::requestChecksums(
    const QnUuid& peer,
    const QString& fileName,
    AbstractPeerManager::ChecksumsCallback callback)
{
    m_requestCounter.incrementCounters(peer, RequestCounter::ChecksumsRequest);
    return m_peerManager->requestChecksums(peer, fileName, callback);
}

rest::Handle ProxyTestPeerManager::downloadChunk(
    const QnUuid& peer,
    const QString& fileName,
    int chunkIndex,
    AbstractPeerManager::ChunkCallback callback)
{
    m_requestCounter.incrementCounters(peer, RequestCounter::DownloadChunkRequest);
    return m_peerManager->downloadChunk(peer, fileName, chunkIndex, callback);
}

rest::Handle ProxyTestPeerManager::downloadChunkFromInternet(const QnUuid& peerId, const QString& fileName,
    const utils::Url &url,
    int chunkIndex,
    int chunkSize,
    AbstractPeerManager::ChunkCallback callback)
{
    m_requestCounter.incrementCounters(peerId, RequestCounter::DownloadChunkFromInternetRequest);
    return m_peerManager->downloadChunkFromInternet(
        peerId, fileName, url, chunkIndex, chunkSize, callback);
}

rest::Handle ProxyTestPeerManager::validateFileInformation(
    const FileInformation& fileInformation, ValidateCallback callback)
{
    m_requestCounter.incrementCounters(selfId(), RequestCounter::DownloadChunkFromInternetRequest);
    return m_peerManager->validateFileInformation(fileInformation, callback);
}


void ProxyTestPeerManager::cancelRequest(const QnUuid& peerId, rest::Handle handle)
{
    m_peerManager->cancelRequest(peerId, handle);
}

TestPeerManager::FileInformation::FileInformation(const downloader::FileInformation& fileInfo):
    downloader::FileInformation(fileInfo)
{
}

//-------------------------------------------------------------------------------------------------

void RequestCounter::incrementCounters(
    const QnUuid& peerId, RequestCounter::RequestType requestType)
{
    ++counters[requestType][peerId];
    ++counters[requestType][kNullGuid];
    ++counters[Total][peerId];
    ++counters[Total][kNullGuid];
}

int RequestCounter::totalRequests() const
{
    return counters[Total][kNullGuid];
}

void RequestCounter::printCounters(const QString& header, TestPeerManager* peerManager) const
{
    QTextStream out(stdout);

    auto printSplit =
        [&out]()
        {
            for (int type = FirstRequestType; type < RequestTypesCount; ++type)
                out << "----------";
            out << "-\n";
        };

    out << header << "\n";
    printSplit();
    for (int type = FirstRequestType; type < RequestTypesCount; ++type)
        out << "|  " << requestTypeShortName(static_cast<RequestType>(type)) << "  ";
    out << "|\n";
    printSplit();

    auto peers = counters[Total].keys();
    std::sort(peers.begin(), peers.end());
    for (const auto& peerId: peers)
    {
        for (int type = FirstRequestType; type < RequestTypesCount; ++type)
            out << "| " << QString::number(counters[type][peerId]).rightJustified(7) << " ";
        out << "| "
            << (peerId.isNull() ? "All Peers" : peerManager->peerString(peerId))
            << "\n";
    }

    printSplit();
}

QString RequestCounter::requestTypeShortName(RequestCounter::RequestType requestType)
{
    switch (requestType)
    {
        case FileInfoRequest:
            return "FINFO";
        case ChecksumsRequest:
            return "CHSUM";
        case DownloadChunkRequest:
            return "DOWNL";
        case DownloadChunkFromInternetRequest:
            return "DOWNI";
        case InternetDownloadRequestsPerformed:
            return "IDOWP";
        case Total:
            return "TOTAL";
        default:
            // Should not get here.
            NX_ASSERT(false);
    }

    return QString();
}

} // namespace downloader
} // namespace p2p
} // namespace common
} // namespace vms
} // namespace nx
