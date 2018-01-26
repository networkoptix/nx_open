#include "worker.h"

#include <chrono>

#include <QtCore/QTimer>

#include <nx/utils/log/log.h>
#include <nx/utils/raii_guard.h>
#include <nx/utils/random.h>
#include <nx/fusion/model_functions.h>

#include "storage.h"
#include "abstract_peer_manager.h"

namespace {

using namespace std::chrono;
using nx::vms::common::p2p::downloader::Worker;

constexpr int kDefaultPeersPerOperation = 5;
constexpr int kSubsequentChunksToDownload = 5;
constexpr int kStartDelayMs = milliseconds(seconds(1)).count();
constexpr int kDefaultStepDelayMs = milliseconds(minutes(1)).count();
constexpr int kMaxAutoRank = 5;
constexpr int kMinAutoRank = 0;
constexpr int kDefaultRank = 0;
static const int kMaxSimultaneousDownloads = 10;

QString statusString(bool success)
{
    return success ? lit("OK") : lit("FAIL");
}

QString requestSubjectString(Worker::State state)
{
    switch (state)
    {
        case Worker::State::requestingFileInformation:
            return lit("file info");
        case Worker::State::requestingAvailableChunks:
            return lit("available chunks");
        case Worker::State::requestingChecksums:
            return lit("checksums");
        case Worker::State::downloadingChunks:
            return lit("chunk");
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

    const int currentIndex = std::distance(
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

namespace nx {
namespace vms {
namespace common {
namespace p2p {
namespace downloader {

Worker::Worker(
    const QString& fileName,
    Storage* storage,
    AbstractPeerManager* peerManager,
    QObject* parent)
    :
    QnLongRunnable(parent),
    m_storage(storage),
    m_peerManager(peerManager),
    m_fileName(fileName),
    m_peersPerOperation(kDefaultPeersPerOperation)
{
    updateLogTag();

    NX_ASSERT(storage);
    if (!storage)
        return;

    NX_ASSERT(peerManager);
    if (!peerManager)
        return;

    if (!peerManager->parent())
        peerManager->setParent(this);

    NX_VERBOSE(m_logTag, lm("Created. Self GUID: %1").arg(peerManager->selfId().toString()));
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
    QnMutexLocker lock(&m_mutex);
    pleaseStopUnsafe();
}

void Worker::pleaseStopUnsafe()
{
    m_needStop = true;
    cancelRequests();
    m_waitCondition.wakeOne();
}

Worker::State Worker::state() const
{
    QnMutexLocker lock(&m_mutex);
    return m_state;
}

int Worker::peersPerOperation() const
{
    QnMutexLocker lock(&m_mutex);
    return m_peersPerOperation;
}

void Worker::setPeersPerOperation(int peersPerOperation)
{
    QnMutexLocker lock(&m_mutex);
    m_peersPerOperation = peersPerOperation;
}

bool Worker::haveChunksToDownload()
{
    QnMutexLocker lock(&m_mutex);
    return haveChunksToDownloadUnsafe();
}

bool Worker::haveChunksToDownloadUnsafe()
{
    const auto& fileInfo = fileInformation();
    if (!fileInfo.isValid())
        return false;

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

void Worker::setPreferredPeers(const QList<QnUuid>& preferredPeers)
{
    for (const auto& peerId: preferredPeers)
        m_peerInfoById[peerId].rank = kMaxAutoRank;
}

void Worker::setState(State state)
{
    if (m_state == state)
        return;

    NX_VERBOSE(m_logTag, lm("Entering state %1...").arg(QnLexical::serialized(state)));
    m_state = state;
    m_mutex.unlock();
    emit stateChanged(state);
    m_mutex.lock();
}

void Worker::doWork()
{
    while (true)
    {
        QnMutexLocker lock(&m_mutex);
        waitBeforeNextIteration(&lock);

        if (needToStop())
            return;

        const auto& fileInfo = fileInformation();
        if (!fileInfo.isValid())
            return;

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
                validateFileInformation();
                break;

            case State::fileInformationValidated:
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
                    if (m_usingInternet)
                    {
                        downloadNextChunk();
                    }
                    else if (haveChunksToDownloadUnsafe())
                    {
                        if (m_subsequentChunksToDownload == 0 && needToFindBetterPeers())
                        {
                            if (m_downloadingChunks.count(true) == 0)
                                requestAvailableChunks();
                            else
                                setShouldWaitForCb();
                        }
                        else
                            downloadNextChunk();
                    }
                    else if (m_downloadingChunks.count(true) == 0)
                    {
                        requestAvailableChunks();
                    }
                    else if (m_downloadingChunks.count(true) != 0)
                    {
                        setShouldWaitForCb();
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

void Worker::validateFileInformation()
{
    const auto& fileInfo = fileInformation();
    if (fileInfo.url.isValid() && !m_fileInfoValidated)
    {
        setState(State::validatingFileInformation);
        auto handle = m_peerManager->validateFileInformation(fileInfo,
            [this, self = shared_from_this()](bool success, rest::Handle handle)
            {
                QnMutexLocker lock(&m_mutex);

                auto requestContext = m_contextByHandle.take(handle);
                NX_ASSERT(!requestContext.peerId.isNull());
                NX_ASSERT(m_state == State::validatingFileInformation);

                if (requestContext.cancelled)
                    return;

                if (success)
                    setState(State::fileInformationValidated);
                else
                    fail();

                m_fileInfoValidated = true;
                setShouldWait(false);
            });

        m_contextByHandle.insert(
            handle, RequestContext(m_peerManager->selfId(), State::validatingFileInformation));
        setShouldWaitForCb();
    }
    else
    {
        setState(State::fileInformationValidated);
        setShouldWait(false);
    }
}

void Worker::requestFileInformationInternal()
{
    m_subsequentChunksToDownload = 0;

    NX_VERBOSE(m_logTag, lm("Trying to get %1.").arg(requestSubjectString(m_state)));

    const auto peers = selectPeersForOperation();
    if (peers.isEmpty())
    {
        if (m_state == State::requestingAvailableChunks
            && !selectPeersForInternetDownload().isEmpty())
        {
            setState(State::foundAvailableChunks);
            return setShouldWait(false);
        }

        return setShouldWait(true);
    }

    for (const auto& peer: peers)
    {
        NX_VERBOSE(m_logTag,
            lm("Requesting %1 from server %2.")
                .arg(requestSubjectString(m_state))
                .arg(m_peerManager->peerString(peer)));

        const auto handle = m_peerManager->requestFileInfo(peer, m_fileName,
            [this, self = shared_from_this()](
                bool success, rest::Handle handle, const FileInformation& fileInfo)
            {
                handleFileInformationReply(success, handle, fileInfo);
            });
        if (handle > 0)
            m_contextByHandle[handle] = RequestContext(peer, State::requestingFileInformation);
    }

    if (m_contextByHandle.isEmpty())
        setShouldWait(true);
    else
        setShouldWaitForCb();
}

void Worker::setShouldWaitForCb()
{
    m_delay = 0;
    m_shouldWait = true;
}

void Worker::setShouldWait(bool value)
{
    if (value && !m_stepDelayTimer.hasPendingTasks())
    {
        m_shouldWait = true;
        m_stepDelayTimer.addTimer(
            [this](utils::TimerId /*timerId*/)
            {
                QnMutexLocker lock(&m_mutex);
                m_shouldWait = false;
                m_waitCondition.wakeOne();
            },
            std::chrono::milliseconds(delayMs()));
    }
    else
    {
        m_shouldWait = false;
        m_waitCondition.wakeOne();
    }
}

qint64 Worker::delayMs() const
{
    return kDefaultStepDelayMs;
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
    bool success, rest::Handle handle, const FileInformation& fileInfo)
{
    QnMutexLocker lock(&m_mutex);

    auto requestContext = m_contextByHandle.take(handle);
    NX_ASSERT(!requestContext.peerId.isNull());
    NX_ASSERT(requestContext.type == State::requestingFileInformation);

    if (requestContext.cancelled)
        return;

    NX_VERBOSE(m_logTag,
        lm("Got %3 reply from %1: %2")
            .arg(m_peerManager->peerString(requestContext.peerId))
            .arg(statusString(success))
            .arg(requestSubjectString(m_state)));

    auto exitGuard = QnRaiiGuard::createDestructible(
        [this]()
        {
            if (!m_contextByHandle.isEmpty())
                return setShouldWaitForCb();

            if (m_state == State::requestingFileInformation
                || m_state == State::requestingAvailableChunks)
            {
                if (isFileReadyForInternetDownload()
                    && !selectPeersForInternetDownload().isEmpty())
                {

                    setState(State::foundAvailableChunks);
                    setShouldWait(false);
                    return;
                }

                setShouldWait(true);
                return;
            }

            setShouldWait(false);
    });

    auto& peerInfo = m_peerInfoById[requestContext.peerId];

    if (!success || !fileInfo.isValid())
    {
        decreasePeerRank(requestContext.peerId);
        return;
    }

    if (m_state == State::requestingFileInformation
        && fileInfo.size >= 0 && !fileInfo.md5.isEmpty() && fileInfo.chunkSize > 0)
    {
        auto resultCode = m_storage->setChunkSize(fileInfo.name, fileInfo.chunkSize);
        if (resultCode != ResultCode::ok)
        {
            NX_WARNING(m_logTag,
                lm("During setting chunk size storage returned error: %1")
                    .arg(resultCode));

            fail();
            return;
        }

        resultCode = m_storage->updateFileInformation(
            fileInfo.name, fileInfo.size, fileInfo.md5);

        if (resultCode != ResultCode::ok)
        {
            NX_WARNING(m_logTag,
                lm("During update storage returned error: %1").arg(resultCode));
        }
        NX_INFO(m_logTag, "Updated file info.");

        setState(State::foundFileInformation);
    }

    peerInfo.downloadedChunks = fileInfo.downloadedChunks;
    if (m_availableChunks.count(true) < fileInfo.downloadedChunks.size()
        && !addAvailableChunksInfo(fileInfo.downloadedChunks))
    {
        decreasePeerRank(requestContext.peerId);
        return;
    }

    increasePeerRank(requestContext.peerId);
    if (m_state == State::requestingAvailableChunks)
        setState(State::foundAvailableChunks);
}

void Worker::requestChecksums()
{
    if (m_downloadingChunks.count(true) != 0)
        return setShouldWaitForCb();

    m_subsequentChunksToDownload = 0;
    setState(State::requestingChecksums);

    const auto peers = selectPeersForOperation();
    if (peers.isEmpty())
    {
        setShouldWait(true);
        return;
    }

    for (const auto& peer: peers)
    {
        NX_VERBOSE(m_logTag,
            lm("Requesting %1 from server %2.")
                .arg(requestSubjectString(State::requestingChecksums))
                .arg(m_peerManager->peerString(peer)));

        const auto handle = m_peerManager->requestChecksums(peer, m_fileName,
            [this, self = shared_from_this()](
                bool success, rest::Handle handle, const QVector<QByteArray>& checksums)
            {
                handleChecksumsReply(success, handle, checksums);
            });
        if (handle > 0)
            m_contextByHandle[handle] = RequestContext(peer, State::requestingChecksums);
    }

    if (m_contextByHandle.isEmpty())
        setShouldWait(true);
    else
        setShouldWaitForCb();
}

void Worker::handleChecksumsReply(
    bool success, rest::Handle handle, const QVector<QByteArray>& checksums)
{
    QnMutexLocker lock(&m_mutex);

    auto requestContext = m_contextByHandle.take(handle);
    NX_ASSERT(!requestContext.peerId.isNull());
    NX_ASSERT(requestContext.type == State::requestingChecksums);

    if (requestContext.cancelled)
        return;

    NX_VERBOSE(m_logTag,
        lm("Got %1 from %2: %3")
            .arg(requestSubjectString(State::requestingChecksums))
            .arg(m_peerManager->peerString(requestContext.peerId))
            .arg(statusString(success)));

    auto exitGuard = QnRaiiGuard::createDestructible(
        [this, &success]()
        {
            if (success)
            {
                cancelRequestsByType(State::requestingChecksums);
                setShouldWait(true);
                return;
            }

            setShouldWait(false);
        });

    if (m_state != State::requestingChecksums)
    {
        success = false;
        return;
    }

    auto& peerInfo = m_peerInfoById[requestContext.peerId];

    if (!success || checksums.isEmpty() || checksums.size() != m_availableChunks.size())
    {
        success = false;
        decreasePeerRank(requestContext.peerId);
        return;
    }

    const auto resultCode =
        m_storage->setChunkChecksums(m_fileName, checksums);

    if (resultCode != ResultCode::ok)
    {
        NX_WARNING(m_logTag, lm("Cannot set checksums: %1").arg(resultCode));
        return;
    }

    NX_DEBUG(m_logTag, "Updated checksums.");

    const auto& fileInfo = fileInformation();
    if (fileInfo.status != FileInformation::Status::downloading
        || fileInfo.downloadedChunks.count(true) == fileInfo.downloadedChunks.size())
    {
        fail();
        return;
    }

    setState(State::downloadingChunks);
}

void Worker::downloadNextChunk()
{
    setState(State::downloadingChunks);

    if (m_downloadingChunks.count(true) >= kMaxSimultaneousDownloads)
        return setShouldWaitForCb();

    const int chunkIndex = selectNextChunk();

    if (chunkIndex < 0)
        return setShouldWaitForCb();

    const auto& fileInfo = fileInformation();

    QList<QnUuid> peers;

    if (m_usingInternet && m_subsequentChunksToDownload > 0)
    {
        peers = selectPeersForInternetDownload();
    }
    else
    {
        peers = peersForChunk(chunkIndex);
        if (peers.isEmpty() && fileInfo.url.isValid())
        {
            m_usingInternet = true;
            peers = selectPeersForInternetDownload();
        }
    }

    peers = selectPeersForOperation(1, peers);
    if (peers.isEmpty())
        return setShouldWait(true);

    NX_ASSERT(peers.size() == 1);

    const auto& peerId = peers.first();

    NX_VERBOSE(m_logTag,
        lm("Selected peer %1, rank: %2, distance: %3")
            .arg(m_peerManager->peerString(peerId))
            .arg(m_peerInfoById[peerId].rank)
            .arg(m_peerManager->distanceTo(peerId)));

    auto handleReply =
        [this, chunkIndex, self = shared_from_this()](
            bool result,
            rest::Handle handle,
            const QByteArray& data)
        {
            handleDownloadChunkReply(result, handle, chunkIndex, data);
        };

    if (m_subsequentChunksToDownload == 0)
    {
        if (m_usingInternet && peerId != m_peerManager->selfId())
            m_subsequentChunksToDownload = 1;
        else
            m_subsequentChunksToDownload = kSubsequentChunksToDownload;
    }

    m_downloadingChunks.setBit(chunkIndex, true);

    rest::Handle handle;

    if (m_usingInternet)
    {
        NX_VERBOSE(m_logTag,
            lm("Requesting chunk %1 from the Internet via %2...")
                .arg(chunkIndex).arg(m_peerManager->peerString(peerId)));
        handle = m_peerManager->downloadChunkFromInternet(
            peerId, fileInfo.name, fileInfo.url, chunkIndex, fileInfo.chunkSize, handleReply);
    }
    else
    {
        NX_VERBOSE(m_logTag,
            lm("Requesting chunk %1 from %2...")
                .arg(chunkIndex).arg(m_peerManager->peerString(peerId)));
        handle = m_peerManager->downloadChunk(peerId, m_fileName, chunkIndex, handleReply);
    }

    if (handle < 0)
    {
        NX_VERBOSE(m_logTag,
            lm("Cannot send request for chunk %1 to %2...")
                .arg(chunkIndex).arg(m_peerManager->peerString(peerId)));
    }

    if (handle > 0)
        m_contextByHandle[handle] = RequestContext(peerId, State::downloadingChunks);
}

void Worker::handleDownloadChunkReply(
    bool success, rest::Handle handle, int chunkIndex, const QByteArray& data)
{
    QnMutexLocker lock(&m_mutex);

    auto requestContext = m_contextByHandle.take(handle);
    NX_ASSERT(!requestContext.peerId.isNull());
    NX_ASSERT(requestContext.type == State::downloadingChunks);

    if (requestContext.cancelled)
        return;

    auto exitGuard = QnRaiiGuard::createDestructible(
        [this, &success, &lock]()
        {
            setShouldWait(!success);
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

    NX_VERBOSE(m_logTag,
        lm("Got chunk %1 from %2: %3")
            .arg(chunkIndex)
            .arg(m_peerManager->peerString(requestContext.peerId))
            .arg(statusString(success)));

    if (!success)
    {
        decreasePeerRank(requestContext.peerId);
        return;
    }

    m_subsequentChunksToDownload = std::max(0, m_subsequentChunksToDownload - 1);
    if (m_subsequentChunksToDownload == 0)
        m_usingInternet = false;

    const auto resultCode = m_storage->writeFileChunk(m_fileName, chunkIndex, data);
    if (resultCode != ResultCode::ok)
    {
        NX_WARNING(m_logTag,
            lm("Cannot write chunk. Storage error: %1").arg(resultCode));

        // TODO: Implement error handling
        success = false;
        return;
    }
}

void Worker::cancelRequests()
{
    for (auto it = m_contextByHandle.begin(); it != m_contextByHandle.end(); ++it)
    {
        m_peerManager->cancelRequest(it.value().peerId, it.key());
        it->cancelled = true;
    }
}

void Worker::cancelRequestsByType(State type)
{
    for (auto it = m_contextByHandle.begin(); it != m_contextByHandle.end(); ++it)
    {
        if (it->type == type)
        {
            m_peerManager->cancelRequest(it.value().peerId, it.key());
            it->cancelled = true;
        }
    }
}

void Worker::finish()
{
    setState(State::finished);
    NX_INFO(m_logTag, "Download finished.");
    m_mutex.unlock();
    emit finished(m_fileName);
    m_mutex.lock();
}

void Worker::fail()
{
    setState(State::failed);
    NX_ERROR(m_logTag, "Download failed.");
    m_mutex.unlock();
    emit failed(m_fileName);
    m_mutex.lock();
}

void Worker::updateLogTag()
{
    auto tag = lit(" [%1]").arg(m_fileName);
    if (m_printSelfPeerInLogs)
        tag.prepend(lit("Worker [%1]").arg(m_peerManager->peerString(m_peerManager->selfId())));
    else
        tag.prepend(::toString(this));

    m_logTag = nx::utils::log::Tag(tag);
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

QList<QnUuid> Worker::peersForChunk(int chunkIndex) const
{
    QList<QnUuid> result;

    for (auto it = m_peerInfoById.begin(); it != m_peerInfoById.end(); ++it)
    {
        if (it->downloadedChunks.size() > chunkIndex && it->downloadedChunks[chunkIndex])
            result.append(it.key());
    }

    return result;
}

FileInformation Worker::fileInformation() const
{
    const auto& fileInfo = m_storage->fileInformation(m_fileName);
    NX_ASSERT(fileInfo.isValid());

    if (fileInfo.downloadedChunks.isEmpty() && !m_availableChunks.isEmpty())
        NX_ASSERT(fileInfo.downloadedChunks.size() == m_availableChunks.size());

    return fileInfo;
}

QList<QnUuid> Worker::peersWithInternetConnection() const
{
    QList<QnUuid> result;

    for (const auto& peerId: m_peerManager->getAllPeers())
    {
        if (m_peerManager->hasInternetConnection(peerId))
            result.append(peerId);
    }

    return result;
}

QList<QnUuid> Worker::selectPeersForOperation(int count, QList<QnUuid> peers) const
{
    QList<QnUuid> result;

    auto exitLogGuard = QnRaiiGuard::createDestructible(
        [this, &result]
        {
            if (result.isEmpty())
                NX_VERBOSE(m_logTag, "No peers selected.");
        });

    if (peers.isEmpty())
        peers = m_peerManager->peers();

    if (count <= 0)
        count = m_peersPerOperation;

    if (peers.size() <= count)
    {
        result = peers;
        return result;
    }

    struct ScoredPeer
    {
        QnUuid peerId;
        int score;
    };

    QList<ScoredPeer> scoredPeers;

    constexpr int kBigDistance = 4;

    for (const auto& peerId: peers)
    {
        const int rank = m_peerInfoById.value(peerId).rank;
        const int distance = m_peerManager->distanceTo(peerId);

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

    const auto selfId = m_peerManager->selfId();

    // Take (3) highest-score peers.
    std::sort(highestScorePeers.begin(), highestScorePeers.end());
    result = takeClosestPeerIds(highestScorePeers, highestScorePeersNeeded, selfId);

    if (result.size() == count)
        return result;

    additionalPeers = count - result.size();

    peers += highestScorePeers;
    std::sort(peers.begin(), peers.end());

    // Take (1) one closest-guid peer.
    result += takeClosestPeerIds(peers, 1, selfId);
    --additionalPeers;

    // Take (2) random peers.
    while (additionalPeers-- > 0)
        result += takeRandomPeers(peers, 1);

    return result;
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

bool Worker::isInternetAvailable(const QList<QnUuid>& peers) const
{
    if (!peers.isEmpty())
    {
        for (const auto& peerId: peers)
        {
            if (m_peerManager->hasInternetConnection(peerId))
                return true;
        }
    }
    else
    {
        if (m_peerManager->hasInternetConnection(m_peerManager->selfId()))
            return true;

        for (const auto& peerId: m_peerManager->getAllPeers())
        {
            if (m_peerManager->hasInternetConnection(peerId))
                return true;
        }
    }

    return false;
}

bool Worker::isFileReadyForInternetDownload() const
{
    const auto& fileInfo = fileInformation();
    return fileInfo.size > 0
        && !fileInfo.md5.isEmpty()
        && fileInfo.chunkSize > 0
        && fileInfo.url.isValid()
        && !m_availableChunks.isEmpty();
}

QList<QnUuid> Worker::selectPeersForInternetDownload() const
{
    if (m_peerManager->hasInternetConnection(m_peerManager->selfId()))
        return {m_peerManager->selfId()};

    const auto& peers = peersWithInternetConnection();

    const auto& currentPeersWithInternet = peers.toSet() & m_peerManager->peers().toSet();

    if (!currentPeersWithInternet.isEmpty())
        return currentPeersWithInternet.toList();

    return peers;
}

bool Worker::needToFindBetterPeers() const
{
    if (m_peerManager->peers().size() < m_peersPerOperation * 2)
        return false;

    auto closestPeers = m_peerManager->peers();
    std::sort(closestPeers.begin(), closestPeers.end());
    closestPeers = takeClosestPeerIds(
        closestPeers, m_peersPerOperation, m_peerManager->selfId());

    for (const auto& peerId: closestPeers)
    {
        if (m_peerInfoById[peerId].rank < kMaxAutoRank - 1)
            return true;
    }

    return false;
}

void Worker::waitBeforeNextIteration(QnMutexLockerBase* lock)
{
    NX_VERBOSE(m_logTag, lm("Start waiting for the next iteration."));

    if (!m_shouldWait || needToStop())
        return;

    while (m_shouldWait && !needToStop())
        m_waitCondition.wait(lock->mutex());
}

void Worker::increasePeerRank(const QnUuid& peerId, int value)
{
    auto& peerInfo = m_peerInfoById[peerId];
    peerInfo.increaseRank(value);
    NX_VERBOSE(m_logTag,
        lm("Increasing rank of %1: %2")
            .arg(m_peerManager->peerString(peerId))
            .arg(peerInfo.rank));
}

void Worker::decreasePeerRank(const QnUuid& peerId, int value)
{
    auto& peerInfo = m_peerInfoById[peerId];
    peerInfo.decreaseRank(value);
    NX_VERBOSE(m_logTag,
        lm("Decreasing rank of %1: %2")
            .arg(m_peerManager->peerString(peerId))
            .arg(peerInfo.rank));
}

void Worker::setPrintSelfPeerInLogs()
{
    m_printSelfPeerInLogs = true;
    updateLogTag();
}

qint64 Worker::defaultStepDelay()
{
    return kDefaultStepDelayMs;
}

AbstractPeerManager* Worker::peerManager() const
{
    return m_peerManager;
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

} // namespace downloader
} // namespace p2p
} // namespace common
} // namespace vms
} // namespace nx
