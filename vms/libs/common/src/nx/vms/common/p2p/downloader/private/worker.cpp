#include "worker.h"

#include <chrono>

#include <QtCore/QTimer>

#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/random.h>
#include <nx/fusion/model_functions.h>

#include "storage.h"
#include "abstract_peer_manager.h"

namespace {

using namespace std::chrono;
using namespace nx::vms::common::p2p::downloader;

constexpr int kDefaultPeersPerOperation = 5;
constexpr int kSubsequentChunksToDownload = 5;
constexpr milliseconds kDefaultStepDelayMs = 1min;
constexpr int kMaxAutoRank = 5;
constexpr int kMinAutoRank = -1;
static const int kMaxSimultaneousDownloads = 5;

QString statusString(bool success)
{
    return success ? "OK" : "FAIL";
}

QString requestSubjectString(Worker::State state)
{
    switch (state)
    {
        case Worker::State::requestingFileInformation:
            return "file info";
        case Worker::State::requestingAvailableChunks:
            return "available chunks";
        case Worker::State::requestingChecksums:
            return "checksums";
        case Worker::State::downloadingChunks:
            return "chunk";
        default:
            break;
    }
    return QString();
}

// Take count elements from a sorted circular list of peers.
QList<QnUuid> takeClosestPeerIds(QList<QnUuid>& peerIds, int count, const QnUuid& referenceId)
{
    QList<QnUuid> result;

    const int size = peerIds.size();

    if (size <= count)
    {
        result.swap(peerIds);
        return result;
    }

    const auto currentIndex = (int) std::distance(
        peerIds.begin(), std::lower_bound(peerIds.begin(), peerIds.end(), referenceId));

    int index = (currentIndex - (count + 1) / 2 + size) % size;
    for (int i = 0; i < count; ++i)
    {
        result.append(peerIds.takeAt(index));

        if (index == peerIds.size())
            index = 0;
    }

    return result;
}

QList<QnUuid> takeRandomPeers(QList<QnUuid>& peers, int count)
{
    QList<QnUuid> result;

    const int size = peers.size();

    if (size <= count)
    {
        result = peers;
        peers.clear();
        return result;
    }

    for (int i = 0; i < count; ++i)
        result.append(peers.takeAt(nx::utils::random::number(0, peers.size() - 1)));

    return result;
}

} // namespace

namespace nx::vms::common::p2p::downloader {

Worker::Worker(
    const QString& fileName,
    Storage* storage,
    const QList<AbstractPeerManager*>& peerManagers,
    const QnUuid& selfId)
    :
    m_selfId(selfId),
    m_storage(storage),
    m_peerManagers(peerManagers),
    m_fileName(fileName),
    m_peersPerOperation(kDefaultPeersPerOperation)
{
    m_logTag = nx::utils::log::Tag(QStringLiteral("%1 [%2]").arg(::toString(this), m_fileName));

    NX_ASSERT(storage);
    if (!storage)
        return;

    NX_ASSERT(!peerManagers.isEmpty());

    NX_VERBOSE(m_logTag, "Created.");
}

Worker::~Worker()
{
    stop();
    NX_VERBOSE(m_logTag, "Deleted.");
}

void Worker::run()
{
    if (m_started)
        return;

    NX_INFO(m_logTag, "Starting...");

    const auto& fileInfo = fileInformation();
    if (!fileInfo.isValid())
        return;

    m_availableChunks.resize(fileInfo.downloadedChunks.size());
    m_downloadingChunks.resize(fileInfo.downloadedChunks.size());

    m_started = true;
    doWork();
}

void Worker::pleaseStop()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    pleaseStopUnsafe();
}

void Worker::pleaseStopUnsafe()
{
    m_needStop = true;
    cancelRequests();
    m_waitCondition.wakeOne();
    m_stepDelayTimer.stop();
}

Worker::State Worker::state() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_state;
}

int Worker::peersPerOperation() const
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return m_peersPerOperation;
}

void Worker::setPeersPerOperation(int peersPerOperation)
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    m_peersPerOperation = peersPerOperation;
}

bool Worker::haveChunksToDownload()
{
    NX_MUTEX_LOCKER lock(&m_mutex);
    return haveChunksToDownloadUnsafe();
}

bool Worker::haveChunksToDownloadUnsafe()
{
    const auto& fileInfo = fileInformation();
    if (!fileInfo.isValid())
        return false;

    for (const PeerInformation& peer: m_peerInfoByPeer)
    {
        if (peer.isInternet && peer.rank > kMinAutoRank)
            return true;
    }

    const int chunksCount = fileInfo.downloadedChunks.size();
    if (m_availableChunks.size() != chunksCount)
        return false;

    for (int i = 0; i < chunksCount; ++i)
    {
        if (m_availableChunks[i] && !fileInfo.downloadedChunks[i] && !m_downloadingChunks[i])
            return true;
    }
    return false;
}

void Worker::setState(State state)
{
    if (m_state == state)
        return;

    NX_VERBOSE(m_logTag, "Entering state %1...", state);
    m_state = state;
    m_mutex.unlock();
    emit stateChanged(state);
    m_mutex.lock();
}


bool Worker::hasNotDownloadingChunks() const
{
    for (int i = 0; i < m_downloadingChunks.size(); ++i)
    {
        if (m_downloadingChunks[i] == false && fileInformation().downloadedChunks[i] == false)
            return true;
    }

    return false;
}

void Worker::doWork()
{
    while (true)
    {
        NX_MUTEX_LOCKER lock(&m_mutex);
        waitBeforeNextIteration(&lock);

        if (needToStop())
        {
            NX_VERBOSE(m_logTag, "doWork(): needToStop() returned true. Exiting...");
            return;
        }

        const auto& fileInfo = fileInformation();
        if (!fileInfo.isValid())
        {
            NX_VERBOSE(m_logTag, "doWork(): File information is not valid. Exiting...");
            return;
        }

        NX_VERBOSE(m_logTag, "doWork(): Start iteration in state %1", m_state);

        switch (m_state)
        {
            case State::initial:
                if (fileInfo.status == FileInformation::Status::downloaded)
                    finish();
                else if (fileInfo.size < 0 || fileInfo.md5.isEmpty())
                    requestFileInformation();
                else if (fileInfo.status == FileInformation::Status::corrupted)
                    requestChecksums();
                else if (!haveChunksToDownloadUnsafe())
                    requestAvailableChunks();
                else if (fileInfo.status == FileInformation::Status::downloading)
                    downloadNextChunk();
                else
                    NX_ASSERT(false, "Invalid state.");
                break;

            case State::requestingFileInformation:
            case State::requestingAvailableChunks:
                requestFileInformationInternal();
                break;

            case State::foundFileInformation:
                if (haveChunksToDownloadUnsafe())
                {
                    downloadNextChunk();
                }
                else
                {
                    setState(State::requestingAvailableChunks);
                    setShouldWait(true);
                }
                break;

            case State::foundAvailableChunks:
                downloadNextChunk();
                break;

            case State::requestingChecksums:
                requestChecksums();
                break;

            case State::downloadingChunks:
                switch (fileInfo.status)
                {
                case FileInformation::Status::downloaded:
                    finish();
                    break;
                case FileInformation::Status::corrupted:
                    requestChecksums();
                    break;
                case FileInformation::Status::downloading:
                    if (haveChunksToDownloadUnsafe())
                    {
                        if (m_subsequentChunksToDownload == 0 && needToFindBetterPeers())
                        {
                            if (m_downloadingChunks.count(true) == 0)
                                requestAvailableChunks();
                            else
                                setShouldWaitForAsyncOperationCompletion();
                        }
                        else
                        {
                            downloadNextChunk();
                        }
                    }
                    else if (m_downloadingChunks.count(true) == 0)
                    {
                        requestAvailableChunks();
                    }
                    else if (m_downloadingChunks.count(true) != 0)
                    {
                        setShouldWaitForAsyncOperationCompletion();
                    }
                    else
                    {
                        NX_ASSERT(false);
                    }

                    break;
                default:
                    NX_ASSERT(false, "Should never get here.");
                }
                break;

            case State::finished:
            case State::failed:
                pleaseStopUnsafe();
                break;

            default:
                NX_ASSERT(false, "Should never get here.");
                break;
        }
    }
}

bool Worker::hasPendingRequestsByType(State type) const
{
    for (const auto& context: m_contextByHandle)
    {
        if (context.type == type)
            return true;
    }

    return false;
}

void Worker::requestFileInformationInternal()
{
    m_subsequentChunksToDownload = 0;

    NX_VERBOSE(m_logTag, "Trying to get %1.", requestSubjectString(m_state));

    const QList<Peer>& peers = selectPeersForOperation(AbstractPeerManager::FileInfo);
    if (peers.isEmpty())
    {
        revivePeersWithMinimalRank();
        setShouldWait(true);
        return;
    }

    const nx::utils::Url& url = fileInformation().url;

    for (const auto& peer: peers)
    {
        NX_VERBOSE(m_logTag, "Requesting %1 from server %2.",
            requestSubjectString(m_state), peer);

        const rest::Handle handle = peer.manager->requestFileInfo(
            peer.id,
            m_fileName,
            url,
            nx::utils::guarded(this,
                [this, peer](bool success, rest::Handle handle, const FileInformation& fileInfo)
                {
                    handleFileInformationReply(
                        success, RequestHandle{peer.manager, handle}, fileInfo);
                }));

        if (handle > 0)
        {
            m_contextByHandle[RequestHandle{peer.manager, handle}] =
                RequestContext(peer, State::requestingFileInformation);
        }
    }

    if (m_contextByHandle.isEmpty())
        setShouldWait(true);
    else
        setShouldWaitForAsyncOperationCompletion();
}

qint64 Worker::delayMs() const
{
    return kDefaultStepDelayMs.count();
}

void Worker::requestFileInformation()
{
    setState(State::requestingFileInformation);
    requestFileInformationInternal();
}

void Worker::requestAvailableChunks()
{
    setState(State::requestingAvailableChunks);
    requestFileInformationInternal();
}

void Worker::handleFileInformationReply(
    bool success, RequestHandle handle, const FileInformation& fileInfo)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    RequestContext requestContext = m_contextByHandle.take(handle);

    NX_VERBOSE(m_logTag, "handleFileInformationReply(): Got %3 reply from %1: %2",
        requestContext.peer,
        statusString(success),
        requestSubjectString(m_state));

    NX_ASSERT(requestContext.type == State::requestingFileInformation);

    auto exitGuard = nx::utils::makeScopeGuard(
        [this]()
        {
            if (!m_contextByHandle.isEmpty())
                return setShouldWaitForAsyncOperationCompletion();

            setShouldWait(m_state == State::requestingFileInformation
                || m_state == State::requestingAvailableChunks);
    });

    auto& peerInfo = m_peerInfoByPeer[requestContext.peer];

    if (!success)
    {
        decreasePeerRank(requestContext.peer);
        return;
    }

    peerInfo.isInternet = fileInfo.downloadedChunks.isEmpty();

    if (m_state == State::requestingFileInformation)
    {
        if (fileInfo.size < 0 || fileInfo.md5.isEmpty() || fileInfo.chunkSize <= 0)
        {
            if (!peerInfo.isInternet)
            {
                decreasePeerRank(requestContext.peer);
                return;
            }
        }

        auto resultCode = m_storage->setChunkSize(fileInfo.name, fileInfo.chunkSize);
        if (resultCode != ResultCode::ok)
        {
            NX_WARNING(m_logTag,
                "handleFileInformationReply(): "
                    "During setting chunk size storage returned error: %1",
                resultCode);

            finish(State::failed);
            return;
        }

        resultCode = m_storage->updateFileInformation(
            fileInfo.name, fileInfo.size, fileInfo.md5);

        if (resultCode != ResultCode::ok)
        {
            NX_WARNING(m_logTag,
                "handleFileInformationReply(): "
                    "During update storage returned error: %1",
                resultCode);
        }

        NX_INFO(m_logTag, "handleFileInformationReply(): Updated file info.");

        setState(State::foundFileInformation);
    }

    peerInfo.downloadedChunks = fileInfo.downloadedChunks;
    addAvailableChunksInfo(fileInfo.downloadedChunks);

    increasePeerRank(requestContext.peer);
    if (m_state == State::requestingAvailableChunks)
    {
        if (haveChunksToDownloadUnsafe())
        {
            setState(State::foundAvailableChunks);
        }
        else
        {
            if (m_contextByHandle.isEmpty())
                setShouldWait(true);
            else
                setShouldWaitForAsyncOperationCompletion();
        }
    }
}

void Worker::requestChecksums()
{
    NX_VERBOSE(m_logTag, "requestChecksums()");

    if (const int count = m_downloadingChunks.count(true); count > 0)
    {
        NX_VERBOSE(m_logTag, "requestChecksums(): Still downloading %1 chunk(s). Exiting.", count);
        return setShouldWaitForAsyncOperationCompletion();
    }

    m_subsequentChunksToDownload = 0;
    setState(State::requestingChecksums);

    const auto peers = selectPeersForOperation(AbstractPeerManager::Checksums);
    if (peers.isEmpty())
    {
        NX_VERBOSE(m_logTag, "requestChecksums(): No peers selected. Exiting.");
        revivePeersWithMinimalRank();
        setShouldWait(true);
        return;
    }

    for (const auto& peer: peers)
    {
        NX_VERBOSE(m_logTag, "requestChecksums(): Requesting %1 from server %2.",
            requestSubjectString(State::requestingChecksums), peer);

        const auto handle = peer.manager->requestChecksums(
            peer.id,
            m_fileName,
            nx::utils::guarded(this,
                [this, peer](
                    bool success, rest::Handle handle, const QVector<QByteArray>& checksums)
                {
                    handleChecksumsReply(success, RequestHandle{peer.manager, handle}, checksums);
                }));
        if (handle > 0)
        {
            m_contextByHandle[RequestHandle{peer.manager, handle}] =
                RequestContext(peer, State::requestingChecksums);
        }
    }

    if (m_contextByHandle.isEmpty())
        setShouldWait(true);
    else
        setShouldWaitForAsyncOperationCompletion();
}

void Worker::handleChecksumsReply(
    bool success, RequestHandle handle, const QVector<QByteArray>& checksums)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    auto requestContext = m_contextByHandle.take(handle);

    NX_VERBOSE(m_logTag, "handleChecksumsReply(): Got %1 from %2: %3",
        requestSubjectString(State::requestingChecksums),
        requestContext.peer,
        statusString(success));

    NX_ASSERT(!requestContext.peer.isNull());
    NX_ASSERT(requestContext.type == State::requestingChecksums);

    auto exitGuard = nx::utils::makeScopeGuard(
        [this, &success]()
        {
            if (success)
                cancelRequestsByType(State::requestingChecksums);
            setShouldWait(false);
        });

    if (m_state != State::requestingChecksums)
    {
        success = false;
        return;
    }

    if (!success || checksums.isEmpty() || checksums.size() != m_availableChunks.size())
    {
        NX_VERBOSE(m_logTag, "handleChecksumsReply(): Got invalid reply. Exiting.");
        success = false;
        decreasePeerRank(requestContext.peer);
        return;
    }

    const auto resultCode =
        m_storage->setChunkChecksums(m_fileName, checksums);

    if (resultCode != ResultCode::ok)
    {
        NX_WARNING(m_logTag, "handleChecksumsReply(): Cannot set checksums: %1", resultCode);
        return;
    }

    NX_DEBUG(m_logTag, "handleChecksumsReply(): Updated checksums.");

    const auto& fileInfo = fileInformation();
    if (fileInfo.status != FileInformation::Status::downloading
        || fileInfo.downloadedChunks.count(true) == fileInfo.downloadedChunks.size())
    {
        finish(State::failed);
        return;
    }

    setState(State::downloadingChunks);
}

void Worker::downloadNextChunk()
{
    NX_VERBOSE(m_logTag, "downloadNextChunk()");

    setState(State::downloadingChunks);

    if (m_downloadingChunks.count(true) >= kMaxSimultaneousDownloads)
    {
        NX_VERBOSE(m_logTag,
            "downloadNextChunk(): Max number of chunks is already being downloaded.");
        return setShouldWaitForAsyncOperationCompletion();
    }

    const int chunkIndex = selectNextChunk();

    if (chunkIndex < 0)
    {
        NX_VERBOSE(m_logTag, "downloadNextChunk(): chunk index < 0");
        return setShouldWait(false);
    }

    const auto& fileInfo = fileInformation();

    QList<Peer> peers = peersForChunk(chunkIndex);
    if (!NX_ASSERT(!peers.isEmpty()))
    {
        NX_VERBOSE(m_logTag, "downloadNextChunk(): The selected chunk is not available.");
        return setShouldWait(false);
    }

    peers = selectPeersForOperation(AbstractPeerManager::DownloadChunk, 1, peers);
    if (peers.isEmpty())
    {
        NX_VERBOSE(m_logTag, "downloadNextChunk(): No peers are selected for download. Waiting.");
        revivePeersWithMinimalRank();
        return setShouldWait(true);
    }

    const auto& peer = peers.first();

    NX_VERBOSE(m_logTag, "downloadNextChunk(): Selected peer %1, rank: %2, distance: %3",
        peer,
        m_peerInfoByPeer[peer].rank,
        peer.manager->distanceTo(peer.id));

    auto handleReply = nx::utils::guarded(this,
        [this, peer, chunkIndex](
            bool result,
            rest::Handle handle,
            const QByteArray& data)
        {
            handleDownloadChunkReply(
                result, RequestHandle{peer.manager, handle}, chunkIndex, data);
        });

    if (m_subsequentChunksToDownload == 0)
        m_subsequentChunksToDownload = kSubsequentChunksToDownload;

    m_downloadingChunks.setBit(chunkIndex, true);

    NX_VERBOSE(m_logTag, "downloadNextChunk(): Requesting chunk %1 from %2...", chunkIndex, peer);
    const rest::Handle handle = peer.manager->downloadChunk(
        peer.id, m_fileName, fileInfo.url, chunkIndex, (int) fileInfo.chunkSize, handleReply);

    if (handle <= 0)
    {
        NX_VERBOSE(m_logTag, "downloadNextChunk(): Cannot send request for chunk %1 to %2...",
            chunkIndex, peer);
    }
    else
    {
        NX_VERBOSE(m_logTag, "downloadNextChunk(): Issued download request. Handle: %1, peer: %2",
            handle, peer);
        m_contextByHandle[RequestHandle{peer.manager, handle}] =
            RequestContext(peer, State::downloadingChunks);
    }
}

void Worker::handleDownloadChunkReply(
    bool success, RequestHandle handle, int chunkIndex, const QByteArray& data)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    auto requestContext = m_contextByHandle.take(handle);

    NX_VERBOSE(m_logTag,
        "handleDownloadChunkReply(): handle: %1, chunk: %2, success: %3",
        handle,
        chunkIndex,
        success);

    NX_ASSERT(requestContext.type == State::downloadingChunks);

    auto exitGuard = nx::utils::makeScopeGuard(
        [this, &success, &lock]()
        {
            setShouldWait(false);
            if (!success)
            {
                lock.unlock();
                emit chunkDownloadFailed(m_fileName);
            }
        });

    m_downloadingChunks.setBit(chunkIndex, false);

    if (m_state != State::downloadingChunks)
    {
        success = true;
        return;
    }

    NX_VERBOSE(m_logTag, "handleDownloadChunkReply(): Got chunk %1 from %2: %3",
        chunkIndex, requestContext.peer, statusString(success));

    if (m_subsequentChunksToDownload > 0)
        --m_subsequentChunksToDownload;

    if (!success)
    {
        decreasePeerRank(requestContext.peer);
        return;
    }

    const auto resultCode = m_storage->writeFileChunk(m_fileName, chunkIndex, data);
    if (resultCode != ResultCode::ok)
    {
        NX_WARNING(m_logTag, "Cannot write chunk. Storage error: %1", resultCode);

        // TODO: Implement error handling
        success = false;
        return;
    }
}

void Worker::cancelRequests()
{
    NX_DEBUG(m_logTag, "Cancelling all requests...");

    for (auto it = m_contextByHandle.begin(); it != m_contextByHandle.end(); /* no increment */)
    {
        it->peer.manager->cancelRequest(QnUuid(), it.key().handle);
        it = m_contextByHandle.erase(it);
    }
}

void Worker::cancelRequestsByType(State type)
{
    NX_DEBUG(m_logTag, "Cancelling requests of type %1...", type);

    for (auto it = m_contextByHandle.begin(); it != m_contextByHandle.end(); /* no increment */)
    {
        if (it->type == type)
        {
            it->peer.manager->cancelRequest(QnUuid(), it.key().handle);
            it = m_contextByHandle.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void Worker::finish(State state)
{
    setState(State::finished);
    NX_INFO(m_logTag, "Download finished: %1.", state);
    m_mutex.unlock();
    emit finished(m_fileName);
    m_mutex.lock();
}

bool Worker::addAvailableChunksInfo(const QBitArray& chunks)
{
    const auto chunksCount = chunks.size();
    if (chunksCount == 0)
        return false;

    m_availableChunks.resize(chunksCount);

    bool newChunksAvailable = false;

    for (int i = 0; i < chunksCount; ++i)
    {
        QBitRef bit = m_availableChunks[i];
        if (chunks[i] && !bit)
        {
            bit = true;
            newChunksAvailable = true;
        }
    }

    return newChunksAvailable;
}

QList<Worker::Peer> Worker::peersForChunk(int chunkIndex) const
{
    QList<Peer> result;

    for (auto it = m_peerInfoByPeer.begin(); it != m_peerInfoByPeer.end(); ++it)
    {
        if (it->isInternet
            || (it->downloadedChunks.size() > chunkIndex && it->downloadedChunks[chunkIndex]))
        {
            result.append(it.key());
        }
    }

    return result;
}

void Worker::setShouldWaitForAsyncOperationCompletion()
{
    NX_VERBOSE(m_logTag, "setShouldWaitForAsyncOperationCompletion()");
    m_shouldWait = true;
}

void Worker::setShouldWait(bool value)
{
    if (value)
    {
        NX_VERBOSE(m_logTag, "setShouldWait(): Start waiting.");

        m_shouldWait = true;
        if (!m_stepDelayTimer.hasPendingTasks())
        {
            NX_VERBOSE(m_logTag, "setShouldWait(): Starting a timer.");

            m_stepDelayTimer.addTimer(
                [this](utils::TimerId /*timerId*/)
                {
                    NX_VERBOSE(m_logTag, "setShouldWait(): Waking by a timer...");

                    NX_MUTEX_LOCKER lock(&m_mutex);
                    m_shouldWait = false;
                    m_waitCondition.wakeOne();
                },
                std::chrono::milliseconds(delayMs()));
        }
    }
    else if (!value)
    {
        NX_VERBOSE(m_logTag, "setShouldWait(): Waking...");

        m_shouldWait = false;
        m_waitCondition.wakeOne();
    }
}

FileInformation Worker::fileInformation() const
{
    const auto& fileInfo = m_storage->fileInformation(m_fileName);
    NX_ASSERT(fileInfo.isValid());

    if (fileInfo.downloadedChunks.isEmpty() && !m_availableChunks.isEmpty())
        NX_ASSERT(fileInfo.downloadedChunks.size() == m_availableChunks.size());

    return fileInfo;
}

QList<Worker::Peer> Worker::selectPeersForOperation(
    AbstractPeerManager::Capability capability, int count, const QList<Peer>& allowedPeers) const
{
    QList<Peer> result;

    if (count <= 0)
        count = m_peersPerOperation;

    const auto getAllowedPeerIds =
        [&allowedPeers](AbstractPeerManager* peerManager)
        {
            QList<QnUuid> result;
            for (const Peer& peer: allowedPeers)
            {
                if (peer.manager == peerManager)
                    result.append(peer.id);
            }
            return result;
        };

    for (AbstractPeerManager* peerManager: m_peerManagers)
    {
        if (!peerManager->capabilities.testFlag(capability))
            continue;

        QList<QnUuid> peers = allowedPeers.isEmpty()
            ? peerManager->peers()
            : getAllowedPeerIds(peerManager);

        if (peers.isEmpty())
            continue;

        peers = selectPeersForOperation(peerManager, count - result.size(), peers);
        for (const auto& peerId: peers)
            result.append(Peer{peerId, peerManager});

        if (result.size() >= count)
            break;
    }

    if (result.isEmpty())
        NX_VERBOSE(m_logTag, "No peers selected.");

    return result;
}

QList<QnUuid> Worker::selectPeersForOperation(
    AbstractPeerManager* peerManager, int count, QList<QnUuid> peers) const
{
    QList<QnUuid> result;

    if (peers.isEmpty())
        peers = peerManager->peers();

    if (count <= 0)
        count = m_peersPerOperation;

    if (peers.size() <= count)
        result = peers;

    struct ScoredPeer
    {
        QnUuid peerId;
        int score;
    };

    QList<ScoredPeer> scoredPeers;

    constexpr int kBigDistance = 4;

    for (auto it = peers.begin(); it != peers.end(); /* no increment */)
    {
        const QnUuid& peerId = *it;
        Peer peer{peerId, peerManager};

        const int rank = m_peerInfoByPeer.value(peer).rank;
        if (rank <= kMinAutoRank)
        {
            it = peers.erase(it);
            continue;
        }

        ++it;

        const int distance = peerManager->distanceTo(peerId);

        const int score = std::max(0, kBigDistance - distance) * 2 + rank;
        if (score == 0)
            continue;

        scoredPeers.append(ScoredPeer{peerId, score});
    }

    std::sort(scoredPeers.begin(), scoredPeers.end(),
        [](const ScoredPeer& left, const ScoredPeer& right)
        {
            return left.score < right.score;
        });

    /* The result should contain:
       1) One of the closest-id peers: thus every server will try to use its
          guid neighbours which should lead to more uniform peers load in flat networks.
       2) At least one random peer which wasn't selected in the main selection: this is to avoid
          overloading peers which started file downloading and obviously contain chunks
          while other do not yet.
       3) The rest is the most scored peers having the closest IDs.

       (1) and (2) are additionalPeers, so additionalPeers = 2.
       If there're not enought high-score peers then the result should contain
       random peers instead of them.
    */

    int additionalPeers = 2;
    const int highestScorePeersNeeded = count > additionalPeers
        ? count - additionalPeers
        : 1;

    QList<QnUuid> highestScorePeers;

    constexpr int kScoreThreshold = 1;
    int previousScore = 0;
    while (highestScorePeers.size() < highestScorePeersNeeded && !scoredPeers.isEmpty())
    {
        const int currentScore = scoredPeers.last().score;
        if (currentScore < previousScore && previousScore - currentScore > kScoreThreshold)
            break;

        while (!scoredPeers.isEmpty() && scoredPeers.last().score == currentScore)
        {
            const auto peerId = scoredPeers.takeLast().peerId;
            peers.removeOne(peerId);
            highestScorePeers.prepend(peerId);
        }

        previousScore = currentScore;
    }

    // Take (3) highest-score peers.
    std::sort(highestScorePeers.begin(), highestScorePeers.end());
    result = takeClosestPeerIds(highestScorePeers, highestScorePeersNeeded, m_selfId);

    if (result.size() == count)
        return result;

    additionalPeers = count - result.size();

    peers += highestScorePeers;
    std::sort(peers.begin(), peers.end());

    // Take (1) one closest-guid peer.
    result += takeClosestPeerIds(peers, 1, m_selfId);
    --additionalPeers;

    // Take (2) random peers.
    while (additionalPeers-- > 0)
        result += takeRandomPeers(peers, 1);

    return result;
}

void Worker::revivePeersWithMinimalRank()
{
    NX_DEBUG(m_logTag, "revivePeersWithMinimalRank()");
    for (auto& peerInfo: m_peerInfoByPeer)
    {
        if (peerInfo.rank <= kMinAutoRank)
            ++peerInfo.rank;
    }
}

int Worker::selectNextChunk() const
{
    const auto& fileInfo = fileInformation();
    if (!fileInfo.isValid())
        return -1;

    const int chunksCount = fileInfo.downloadedChunks.size();
    const int randomChunk = utils::random::number(0, chunksCount - 1);

    int firstMetNotDownloadedChunk = -1;

    auto findChunk =
        [this, &fileInfo, &firstMetNotDownloadedChunk](int start, int end)
        {
            for (int i = start; i <= end; ++i)
            {
                if (fileInfo.downloadedChunks[i] || m_downloadingChunks[i])
                    continue;

                if (firstMetNotDownloadedChunk < 0)
                    firstMetNotDownloadedChunk = i;

                if (m_availableChunks[i])
                    return i;
            }

            return -1;
        };

    int chunk = findChunk(randomChunk, chunksCount - 1);
    if (chunk >= 0)
        return chunk;

    chunk = findChunk(0, randomChunk - 1);
    if (chunk >= 0)
        return chunk;

    return firstMetNotDownloadedChunk;
}

bool Worker::needToFindBetterPeers() const
{
    for (AbstractPeerManager* peerManager: m_peerManagers)
    {
        QList<QnUuid> closestPeers = peerManager->peers();
        std::sort(closestPeers.begin(), closestPeers.end());
        closestPeers = takeClosestPeerIds(closestPeers, m_peersPerOperation, m_selfId);

        for (const auto& peerId: closestPeers)
        {
            if (m_peerInfoByPeer[Peer{peerId, peerManager}].rank < kMaxAutoRank - 1)
                return true;
        }
    }

    return false;
}

void Worker::waitBeforeNextIteration(QnMutexLockerBase* lock)
{
    if (!m_shouldWait || needToStop())
    {
        NX_VERBOSE(m_logTag, "Skipped waiting for the next iteration.");
        return;
    }

    NX_VERBOSE(m_logTag, "Start waiting for the next iteration.");

    while (m_shouldWait && !needToStop())
        m_waitCondition.wait(lock->mutex());

    NX_VERBOSE(m_logTag, "Finished waiting for the next iteration.");
}

void Worker::increasePeerRank(const Peer& peer, int value)
{
    auto& peerInfo = m_peerInfoByPeer[peer];
    peerInfo.increaseRank(value);
    NX_VERBOSE(m_logTag, "Increasing rank of %1: %2", peer, peerInfo.rank);
}

void Worker::decreasePeerRank(const Peer& peer, int value)
{
    auto& peerInfo = m_peerInfoByPeer[peer];
    peerInfo.decreaseRank(value);
    NX_VERBOSE(m_logTag, "Decreasing rank of %1: %2", peer, peerInfo.rank);
}

qint64 Worker::defaultStepDelay()
{
    return kDefaultStepDelayMs.count();
}

QList<AbstractPeerManager*> Worker::peerManagers() const
{
    return m_peerManagers;
}

void Worker::PeerInformation::increaseRank(int value)
{
    rank = qBound(kMinAutoRank, rank + value, kMaxAutoRank);
}

void Worker::PeerInformation::decreaseRank(int value)
{
    increaseRank(-value);
}

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(Worker, State)

} // namespace nx::vms::common::p2p::downloader
