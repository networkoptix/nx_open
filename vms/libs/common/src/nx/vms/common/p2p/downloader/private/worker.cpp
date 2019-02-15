#include "worker.h"

#include <chrono>

#include <QtCore/QTimer>

#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/random.h>
#include <nx/fusion/model_functions.h>

#include "storage.h"
#include "abstract_peer_manager.h"

namespace {

using namespace std::chrono;
using nx::vms::common::p2p::downloader::Worker;

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

namespace nx {
namespace vms {
namespace common {
namespace p2p {
namespace downloader {

Worker::Worker(
    const QString& fileName,
    Storage* storage,
    AbstractPeerManager* peerManager)
    :
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

    NX_VERBOSE(m_logTag, "Created. Self GUID: %1", peerManager->selfId());
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
                    if (m_usingInternet)
                    {
                        if (hasNotDownloadingChunks())
                            downloadNextChunk();
                        else
                            setShouldWaitForAsyncOperationCompletion();
                    }
                    else if (haveChunksToDownloadUnsafe())
                    {
                        if (m_subsequentChunksToDownload == 0 && needToFindBetterPeers())
                        {
                            if (m_downloadingChunks.count(true) == 0)
                                requestAvailableChunks();
                            else
                                setShouldWaitForAsyncOperationCompletion();
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
        if (!context.cancelled && context.type == type)
            return true;
    }

    return false;
}

void Worker::requestFileInformationInternal()
{
    m_subsequentChunksToDownload = 0;

    NX_VERBOSE(m_logTag, "Trying to get %1.", requestSubjectString(m_state));

    const auto peers = selectPeersForOperation();
    if (peers.isEmpty())
    {
        revivePeersWithMinimalRank();

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
        QString peerName = m_peerManager->peerString(peer);
        NX_VERBOSE(m_logTag, "Requesting %1 from server %2.",
            requestSubjectString(m_state), peerName);

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
        setShouldWaitForAsyncOperationCompletion();
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
    bool success, rest::Handle handle, const FileInformation& fileInfo)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    auto requestContext = m_contextByHandle.take(handle);

    NX_VERBOSE(m_logTag, "handleFileInformationReply(): Got %3%4 reply from %1: %2",
        m_peerManager->peerString(requestContext.peerId),
        statusString(success),
        requestSubjectString(m_state),
        requestContext.cancelled ? " (cancelled)" : "");

    if (requestContext.cancelled)
        return;

    NX_ASSERT(!requestContext.peerId.isNull());
    NX_ASSERT(requestContext.type == State::requestingFileInformation);

    auto exitGuard = nx::utils::makeScopeGuard(
        [this]()
        {
            if (!m_contextByHandle.isEmpty())
                return setShouldWaitForAsyncOperationCompletion();

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
                "handleFileInformationReply(): "
                    "During setting chunk size storage returned error: %1",
                resultCode);

            fail();
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
    NX_VERBOSE(m_logTag, "requestChecksums()");

    if (const int count = m_downloadingChunks.count(true); count > 0)
    {
        NX_VERBOSE(m_logTag, "requestChecksums(): Still downloading %1 chunk(s). Exiting.", count);
        return setShouldWaitForAsyncOperationCompletion();
    }

    m_subsequentChunksToDownload = 0;
    setState(State::requestingChecksums);

    const auto peers = selectPeersForOperation();
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
            requestSubjectString(State::requestingChecksums),
            m_peerManager->peerString(peer));

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
        setShouldWaitForAsyncOperationCompletion();
}

void Worker::handleChecksumsReply(
    bool success, rest::Handle handle, const QVector<QByteArray>& checksums)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    auto requestContext = m_contextByHandle.take(handle);

    NX_VERBOSE(m_logTag, "handleChecksumsReply(): Got %1 from %2: %3%4",
        requestSubjectString(State::requestingChecksums),
        m_peerManager->peerString(requestContext.peerId),
        statusString(success),
        requestContext.cancelled ? " (cancelled)" : "");

    if (requestContext.cancelled)
        return;

    NX_ASSERT(!requestContext.peerId.isNull());
    NX_ASSERT(requestContext.type == State::requestingChecksums);

    auto exitGuard = nx::utils::makeScopeGuard(
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

    if (!success || checksums.isEmpty() || checksums.size() != m_availableChunks.size())
    {
        NX_VERBOSE(m_logTag, "handleChecksumsReply(): Got invalid reply. Exiting.");
        success = false;
        decreasePeerRank(requestContext.peerId);
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
        fail();
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

    peers = selectPeersForOperation(1, peers, !m_usingInternet);
    if (peers.isEmpty())
    {
        NX_VERBOSE(m_logTag, "downloadNextChunk(): No peers are selected for download. Waiting.");
        revivePeersWithMinimalRank();
        return setShouldWait(true);
    }

    NX_ASSERT(peers.size() == 1);

    const auto& peerId = peers.first();

    NX_VERBOSE(m_logTag, "downloadNextChunk(): Selected peer %1, rank: %2, distance: %3",
        m_peerManager->peerString(peerId),
        m_peerInfoById[peerId].rank,
        m_peerManager->distanceTo(peerId));

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
            "downloadNextChunk(): Requesting chunk %1 from the Internet via %2...",
            chunkIndex,
            m_peerManager->peerString(peerId));
        handle = m_peerManager->downloadChunkFromInternet(
            peerId,
            fileInfo.name,
            fileInfo.url,
            chunkIndex,
            (int) fileInfo.chunkSize,
            handleReply);
    }
    else
    {
        NX_VERBOSE(m_logTag, "downloadNextChunk(): Requesting chunk %1 from %2...",
            chunkIndex, m_peerManager->peerString(peerId));
        handle = m_peerManager->downloadChunk(peerId, m_fileName, chunkIndex, handleReply);
    }

    if (handle <= 0)
    {
        NX_VERBOSE(m_logTag, "downloadNextChunk(): Cannot send request for chunk %1 to %2...",
            chunkIndex, m_peerManager->peerString(peerId));
    }
    else
    {
        NX_VERBOSE(m_logTag,
            "downloadNextChunk(): Issued download request. Handle: %1, peerId: %2",
            handle,
            peerId);
        m_contextByHandle[handle] = RequestContext(peerId, State::downloadingChunks);
    }
}

void Worker::handleDownloadChunkReply(
    bool success, rest::Handle handle, int chunkIndex, const QByteArray& data)
{
    NX_MUTEX_LOCKER lock(&m_mutex);

    auto requestContext = m_contextByHandle.take(handle);

    NX_VERBOSE(m_logTag,
        "handleDownloadChunkReply(): handle: %1, chunk: %2, success: %3, cancelled: %4",
        handle,
        chunkIndex,
        success,
        requestContext.cancelled);

    if (requestContext.cancelled)
        return;

    NX_ASSERT(!requestContext.peerId.isNull());
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
        chunkIndex,
        m_peerManager->peerString(requestContext.peerId),
        statusString(success));

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
        NX_WARNING(m_logTag, "Cannot write chunk. Storage error: %1", resultCode);

        // TODO: Implement error handling
        success = false;
        return;
    }
}

void Worker::cancelRequests()
{
    NX_DEBUG(m_logTag, "Cancelling all requests...");

    for (auto it = m_contextByHandle.begin(); it != m_contextByHandle.end(); ++it)
        it->cancelled = true;
}

void Worker::cancelRequestsByType(State type)
{
    NX_DEBUG(m_logTag, "Cancelling requests of type %1...", type);

    for (auto it = m_contextByHandle.begin(); it != m_contextByHandle.end(); ++it)
    {
        if (it->type == type)
            it->cancelled = true;
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
    auto tag = QStringLiteral(" [%1]").arg(m_fileName);
    if (m_printSelfPeerInLogs)
        tag.prepend(QString("Worker [%1]").arg(m_peerManager->peerString(m_peerManager->selfId())));
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

QList<QnUuid> Worker::selectPeersForOperation(
    int count, QList<QnUuid> peers, bool skipPeersWithMinimalRank) const
{
    QList<QnUuid> result;

    auto exitLogGuard = nx::utils::makeScopeGuard(
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
        if (skipPeersWithMinimalRank)
        {
            for (const auto& peer: peers)
            {
                if (m_peerInfoById[peer].rank > kMinAutoRank)
                    result.append(peer);
            }
        }
        else
        {
            result = peers;
        }
        return result;
    }

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

        const int rank = m_peerInfoById.value(peerId).rank;
        if (skipPeersWithMinimalRank && rank <= kMinAutoRank)
        {
            it = peers.erase(it);
            continue;
        }

        ++it;

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

void Worker::revivePeersWithMinimalRank()
{
    NX_DEBUG(m_logTag, "revivePeersWithMinimalRank()");
    for (auto& peerInfo: m_peerInfoById)
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
    if (m_peerManager->hasInternetConnection(m_peerManager->selfId())
        || m_peerManager->hasAccessToTheUrl(fileInformation().url.toString()))
    {
        return {m_peerManager->selfId()};
    }

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

void Worker::increasePeerRank(const QnUuid& peerId, int value)
{
    auto& peerInfo = m_peerInfoById[peerId];
    peerInfo.increaseRank(value);
    NX_VERBOSE(m_logTag, "Increasing rank of %1: %2",
        m_peerManager->peerString(peerId), peerInfo.rank);
}

void Worker::decreasePeerRank(const QnUuid& peerId, int value)
{
    auto& peerInfo = m_peerInfoById[peerId];
    peerInfo.decreaseRank(value);
    NX_VERBOSE(m_logTag, "Decreasing rank of %1: %2",
        m_peerManager->peerString(peerId), peerInfo.rank);
}

void Worker::setPrintSelfPeerInLogs()
{
    m_printSelfPeerInLogs = true;
    updateLogTag();
}

qint64 Worker::defaultStepDelay()
{
    return kDefaultStepDelayMs.count();
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
