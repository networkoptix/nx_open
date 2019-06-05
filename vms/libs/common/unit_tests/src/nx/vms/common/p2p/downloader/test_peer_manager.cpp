#include "test_peer_manager.h"

#include <thread>

#include <QtCore/QTextStream>

#include <nx/utils/log/assert.h>
#include <nx/vms/common/p2p/downloader/private/storage.h>

namespace nx::vms::common::p2p::downloader {

namespace {

static const auto kNullGuid = QnUuid();
static constexpr int kDefaultRequestTime = 200;

template<typename SharedPromise>
std::function<void()> makeCanceller(
    TestPeerManager* manager, const QnUuid& peerId, rest::Handle handle, SharedPromise promise)
{
    return [manager, peerId, handle, promise]()
        {
            manager->cancelRequest(peerId, handle);
            AbstractPeerManager::setPromiseValueIfEmpty(promise, {});
        };
}

} // namespace

TestPeerManager::TestPeerManager():
    AbstractPeerManager(AllCapabilities)
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

bool TestPeerManager::hasInternetConnection(const QnUuid& peerId) const
{
    return m_peers[peerId].hasInternetConnection;
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

QString TestPeerManager::peerString(const QnUuid& peerId) const
{
    auto result = m_peers[peerId].name;
    if (result.isEmpty())
        result = peerId.toString();

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

AbstractPeerManager::RequestContext<FileInformation> TestPeerManager::requestFileInfo(
    const QnUuid& peerId,
    const QString& fileName,
    const nx::utils::Url& url)
{
    m_requestCounter.incrementCounters(peerId, RequestCounter::FileInfoRequest);

    if (!m_peers.contains(peerId))
        return {};

    auto promise = std::make_shared<std::promise<std::optional<downloader::FileInformation>>>();

    rest::Handle handle = enqueueRequest(peerId, kDefaultRequestTime,
        [this,
            fileInfo = fileInformation(peerId, fileName),
            url,
            peerId,
            promise](
                rest::Handle /*handle*/) mutable
        {
            if (m_allowIndirectInternetRequests && m_peers[peerId].hasInternetConnection)
            {
                fileInfo.downloadedChunks.clear();
                if (url.isValid())
                    setPromiseValueIfEmpty(promise, {fileInfo});
                else
                    setPromiseValueIfEmpty(promise, {});
            }
            else
            {
                if (fileInfo.isValid())
                    setPromiseValueIfEmpty(promise, {fileInfo});
                else
                    setPromiseValueIfEmpty(promise, {});
            }
        });

    return {promise->get_future(), makeCanceller(this, peerId, handle, promise)};
}

AbstractPeerManager::RequestContext<QVector<QByteArray>> TestPeerManager::requestChecksums(
    const QnUuid& peerId, const QString& fileName)
{
    m_requestCounter.incrementCounters(peerId, RequestCounter::ChecksumsRequest);

    auto it = m_peers.find(peerId);
    if (it == m_peers.end())
        return {};

    auto promise = std::make_shared<std::promise<std::optional<QVector<QByteArray>>>>();

    rest::Handle handle = enqueueRequest(peerId, kDefaultRequestTime,
        [promise, fileInfo = fileInformation(peerId, fileName), storage = it->storage](
            rest::Handle /*handle*/)
        {
            if (!fileInfo.isValid())
            {
                setPromiseValueIfEmpty(promise, {});
                return;
            }

            setPromiseValueIfEmpty(promise,
                {storage
                    ? storage->getChunkChecksums(fileInfo.name)
                    : fileInfo.checksums});
        });

    return {promise->get_future(), makeCanceller(this, peerId, handle, promise)};

}

AbstractPeerManager::RequestContext<QByteArray> TestPeerManager::downloadChunk(
    const QnUuid& peerId,
    const QString& fileName,
    const utils::Url &url,
    int chunkIndex,
    int chunkSize)
{
    m_requestCounter.incrementCounters(peerId, RequestCounter::DownloadChunkRequest);

    if (!m_peers.contains(peerId))
        return {};

    auto promise = std::make_shared<std::promise<std::optional<QByteArray>>>();
    rest::Handle handle = -1;

    if (m_allowIndirectInternetRequests)
    {
        handle = enqueueRequest(peerId, kDefaultRequestTime,
            [this, promise, peerId, fileName, url, chunkIndex, chunkSize](rest::Handle /*handle*/)
            {
                Storage* storage = nullptr;
                downloader::FileInformation storageFileInfo;

                auto it = m_peers.find(peerId);
                if (it != m_peers.end() && it->storage)
                {
                    storageFileInfo = it->storage->fileInformation(fileName);
                    if (storageFileInfo.isValid() && storageFileInfo.url == url
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
                        setPromiseValueIfEmpty(promise, {result});
                        return;
                    }
                }

                if (!hasInternetConnection(peerId))
                {
                    setPromiseValueIfEmpty(promise, {});
                    return;
                }

                const auto filePath = m_fileByUrl.value(url);
                if (filePath.isEmpty())
                {
                    setPromiseValueIfEmpty(promise, {});
                    return;
                }

                FileInformation fileInfo;
                fileInfo.filePath = filePath;
                fileInfo.chunkSize = chunkSize;

                auto result = readFileChunk(fileInfo, chunkIndex);
                m_requestCounter.incrementCounters(
                    peerId, RequestCounter::InternetDownloadRequestsPerformed);

                if (result.isNull())
                    setPromiseValueIfEmpty(promise, {});
                else
                    setPromiseValueIfEmpty(promise, {result});
            });
    }
    else
    {
        handle = enqueueRequest(peerId, kDefaultRequestTime,
            [this, promise, fileInfo = fileInformation(peerId, fileName), chunkIndex](
                rest::Handle /*handle*/)
            {
                if (m_downloadFailed)
                {
                    setPromiseValueIfEmpty(promise, {});
                    m_downloadFailed = false;
                    return;
                }

                QByteArray result;
                if (fileInfo.isValid())
                    result = readFileChunk(fileInfo, chunkIndex);

                if (result.isNull())
                    setPromiseValueIfEmpty(promise, {});
                else
                    setPromiseValueIfEmpty(promise, {result});
            });
    }

    return {promise->get_future(), makeCanceller(this, peerId, handle, promise)};
}

void TestPeerManager::setValidateShouldFail()
{
    m_validateShouldFail = true;
}

void TestPeerManager::setOneShotDownloadFail()
{
    m_downloadFailed = true;
}

void TestPeerManager::setDelayBeforeRequest(qint64 delay)
{
    m_delayBeforeRequest = delay;
}

void TestPeerManager::cancelRequest(const QnUuid& peerId, rest::Handle handle)
{
    QnMutexLocker lock(&m_mutex);
    auto it = std::remove_if(m_requestsQueue.begin(), m_requestsQueue.end(),
        [&peerId, &handle](const Request& request)
        {
            return request.peerId == peerId && request.handle == handle;
        });
    m_requestsQueue.erase(it, m_requestsQueue.end());
}

const RequestCounter* TestPeerManager::requestCounter() const
{
    return &m_requestCounter;
}

void TestPeerManager::setIndirectInternetRequestsAllowed(bool allow)
{
    m_allowIndirectInternetRequests = allow;
}

rest::Handle TestPeerManager::getRequestHandle()
{
    return ++m_requestIndex;
}

rest::Handle TestPeerManager::enqueueRequest(
    const QnUuid& peerId, qint64 time, RequestCallback callback)
{
    QnMutexLocker lock(&m_mutex);
    const auto handle = getRequestHandle();
    Request request{peerId, handle, callback, m_currentTime + time};
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
    qint64 requestTime = -1;

    QnMutexLocker lock(&m_mutex);
    while (true)
    {
        while (m_requestsQueue.isEmpty() && !needToStop())
            m_condition.wait(lock.mutex());

        if (needToStop())
            return;

        qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        if (requestTime == -1)
            requestTime = nowMs;

        if (m_delayBeforeRequest != 0 && nowMs - requestTime < m_delayBeforeRequest)
        {
            lock.unlock();
            QThread::msleep(m_delayBeforeRequest - (nowMs - requestTime));
            lock.relock();
        }
        requestTime = nowMs;

        if (m_requestsQueue.isEmpty())
            continue;

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
    AbstractPeerManager(AllCapabilities),
    m_peerManager(peerManager)
{
    NX_ASSERT(peerManager);
    peerManager->addPeer(id, peerName);
}

void ProxyTestPeerManager::calculateDistances()
{
    for (const auto& peerId: m_peerManager->getAllPeers())
        m_distances[peerId] = std::numeric_limits<int>::max();

    QList<QnUuid> peersToCheck{QnUuid()};
    QSet<QnUuid> checkedPeers{QnUuid()};

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

QString ProxyTestPeerManager::peerString(const QnUuid& peerId) const
{
    return m_peerManager->peerString(peerId);
}

QList<QnUuid> ProxyTestPeerManager::getAllPeers() const
{
    return m_peerManager->getAllPeers();
}

int ProxyTestPeerManager::distanceTo(const QnUuid& peerId) const
{
    return m_distances.value(peerId, std::numeric_limits<int>::max());
}

AbstractPeerManager::RequestContext<FileInformation> ProxyTestPeerManager::requestFileInfo(
    const QnUuid& peer,
    const QString& fileName,
    const nx::utils::Url& url)
{
    m_requestCounter.incrementCounters(peer, RequestCounter::FileInfoRequest);
    return m_peerManager->requestFileInfo(peer, fileName, url);
}

AbstractPeerManager::RequestContext<QVector<QByteArray>> ProxyTestPeerManager::requestChecksums(
    const QnUuid& peer, const QString& fileName)
{
    m_requestCounter.incrementCounters(peer, RequestCounter::ChecksumsRequest);
    return m_peerManager->requestChecksums(peer, fileName);
}

AbstractPeerManager::RequestContext<QByteArray> ProxyTestPeerManager::downloadChunk(
    const QnUuid& peerId,
    const QString& fileName,
    const utils::Url &url,
    int chunkIndex,
    int chunkSize)
{
    m_requestCounter.incrementCounters(peerId, RequestCounter::DownloadChunkRequest);
    return m_peerManager->downloadChunk(peerId, fileName, url, chunkIndex, chunkSize);
}

TestPeerManager::FileInformation::FileInformation(const downloader::FileInformation& fileInfo):
    downloader::FileInformation(fileInfo)
{
}

//-------------------------------------------------------------------------------------------------

void RequestCounter::incrementCounters(
    const QnUuid& peerId, RequestCounter::RequestType requestType)
{
    QnMutexLocker lock(&m_mutex);
    ++counters[requestType][peerId];
    ++counters[requestType][kNullGuid];
    ++counters[Total][peerId];
    ++counters[Total][kNullGuid];
}

int RequestCounter::totalRequests() const
{
    QnMutexLocker lock(&m_mutex);
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

} // namespace nx::vms::common::p2p::downloader
