#include "worker.h"

#include <chrono>

#include <QtCore/QTimer>

#include <nx/utils/log/log.h>
#include <nx/utils/raii_guard.h>
#include <nx/fusion/model_functions.h>

#include "storage.h"
#include "abstract_peer_manager.h"

namespace {

using namespace std::chrono;

constexpr int kDefaultPeersPerOperation = 5;
constexpr int kStartDelayMs = milliseconds(seconds(1)).count();
constexpr int kDefaultStepDelayMs = milliseconds(minutes(1)).count();
constexpr int kMaxAutoRank = 5;
constexpr int kMinAutoRank = 0;
constexpr int kDefaultRank = 0;

QString statusString(bool success)
{
    return success ? lit("OK") : lit("FAIL");
}

QList<QnUuid> takeClosestGuidPeers(QList<QnUuid>& peers, int count, const QnUuid& selfId)
{
    QList<QnUuid> result;

    const int size = peers.size();

    if (size <= count)
    {
        result = peers;
        peers.clear();
        return result;
    }

    const int currentIndex = std::distance(
        peers.begin(), std::lower_bound(peers.begin(), peers.end(), selfId));

    const int firstIndex = currentIndex - (count + 1) / 2;
    const int lastIndex = firstIndex + count - 1;

    QVector<int> indexesToTake;
    indexesToTake.reserve(count);

    for (int i = firstIndex; i <= lastIndex; ++i)
    {
        int index = i;

        if (index < 0)
            index += size;
        else if (index >= size)
            index -= size;

        indexesToTake.append(index);
    }
    std::sort(indexesToTake.begin(), indexesToTake.end());

    while (!indexesToTake.isEmpty())
        result.append(peers.takeAt(indexesToTake.takeLast()));

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
        result.append(peers.takeAt(rand() % peers.size()));

    return result;
}

} // namespace

namespace nx {
namespace vms {
namespace common {
namespace distributed_file_downloader {

Worker::Worker(
    const QString& fileName,
    Storage* storage,
    AbstractPeerManager* peerManager,
    QObject* parent)
    :
    base_type(parent),
    m_storage(storage),
    m_peerManager(peerManager),
    m_fileName(fileName),
    m_stepDelayTimer(new QTimer(this)),
    m_peersPerOperation(kDefaultPeersPerOperation)
{
    NX_ASSERT(storage);
    if (!storage)
        return;

    NX_ASSERT(peerManager);
    if (!peerManager)
        return;

    if (!peerManager->parent())
        peerManager->setParent(this);

    m_stepDelayTimer->setSingleShot(true);
    connect(m_stepDelayTimer, &QTimer::timeout, this, &Worker::nextStep);

    NX_LOG(logMessage("Created. Self GUID: %1").arg(peerManager->selfId().toString()),
        cl_logINFO);
}

Worker::~Worker()
{
}

void Worker::start()
{
    if (m_started)
        return;

    NX_LOG(logMessage("Starting..."), cl_logINFO);

    const auto& fileInfo = fileInformation();
    if (!fileInfo.isValid())
        return;

    m_availableChunks.resize(fileInfo.downloadedChunks.size());

    m_started = true;
    waitForNextStep(kStartDelayMs);
}

Worker::State Worker::state() const
{
    return m_state;
}

int Worker::peersPerOperation() const
{
    return m_peersPerOperation;
}

void Worker::setPeersPerOperation(int peersPerOperation)
{
    m_peersPerOperation = peersPerOperation;
}

bool Worker::haveChunksToDownload()
{
    const auto& fileInfo = fileInformation();
    if (!fileInfo.isValid())
        return false;

    const int chunksCount = fileInfo.downloadedChunks.size();
    if (m_availableChunks.size() != chunksCount)
        return false;

    for (int i = 0; i < chunksCount; ++i)
    {
        if (m_availableChunks[i] && !fileInfo.downloadedChunks[i])
            return true;
    }
    return false;
}

QList<QnUuid> Worker::forcedPeers() const
{
    return m_forcedPeers;
}

void Worker::setForcedPeers(const QList<QnUuid>& forcedPeers)
{
    m_forcedPeers = forcedPeers;
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

    NX_LOG(logMessage("Entering state %1...").arg(QnLexical::serialized(state)), cl_logDEBUG1);
    m_state = state;
    emit stateChanged(state);
}

void Worker::nextStep()
{
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
            else if (!haveChunksToDownload())
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
            if (haveChunksToDownload())
            {
                downloadNextChunk();
            }
            else
            {
                setState(State::requestingAvailableChunks);
                waitForNextStep();
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
                    if (haveChunksToDownload())
                        downloadNextChunk();
                    else
                        requestAvailableChunks();
                    break;
                default:
                    NX_ASSERT(false, "Should never get here.");
            }
            break;

        case State::finished:
        case State::failed:
            break;

        default:
            NX_ASSERT(false, "Should never get here.");
            waitForNextStep();
            break;
    }
}

void Worker::requestFileInformationInternal()
{
    const QString subject = m_state == State::requestingAvailableChunks
        ? lit("available chunks")
        : lit("file info");

    NX_LOG(logMessage("Trying to get %1.").arg(subject), cl_logDEBUG2);

    const auto peers = selectPeersForOperation();
    if (peers.isEmpty())
    {
        if (m_state == State::requestingAvailableChunks
            && m_peerManager->hasInternetConnection(m_peerManager->selfId()))
        {
            setState(State::foundAvailableChunks);
            waitForNextStep(0);
            return;
        }

        waitForNextStep();
        return;
    }

    auto handleReply =
        [this, subject](bool success, rest::Handle handle, const FileInformation& fileInfo)
        {
            const auto peerId = m_peerByRequestHandle.take(handle);
            if (peerId.isNull())
                return;

            NX_LOG(
                logMessage("Got %3 reply from %1: %2")
                    .arg(m_peerManager->peerString(peerId))
                    .arg(statusString(success))
                    .arg(subject),
                cl_logDEBUG2);

            auto exitGuard = QnRaiiGuard::createDestructible(
                [this]()
                {
                    if (!m_peerByRequestHandle.isEmpty())
                        return;

                    if (m_state == State::requestingFileInformation
                        || m_state == State::requestingAvailableChunks)
                    {
                        if (!haveInternet())
                        {
                            waitForNextStep();
                            return;
                        }

                        const auto& fileInfo = fileInformation();
                        if (fileInfo.size > 0
                            && !fileInfo.md5.isEmpty()
                            && fileInfo.chunkSize > 0
                            && fileInfo.url.isValid()
                            && !m_availableChunks.isEmpty())
                        {
                            setState(State::foundAvailableChunks);
                            waitForNextStep(0);
                        }

                        return;
                    }

                    waitForNextStep(0);
                });

            auto& peerInfo = m_peerInfoById[peerId];

            if (!success || !fileInfo.isValid())
            {
                peerInfo.decreaseRank();
                return;
            }

            if (m_state == State::requestingFileInformation
                && fileInfo.size >= 0 && !fileInfo.md5.isEmpty() && fileInfo.chunkSize > 0)
            {
                auto errorCode = m_storage->setChunkSize(fileInfo.name, fileInfo.chunkSize);
                if (errorCode != ErrorCode::noError)
                {
                    NX_LOG(
                        logMessage("During setting chunk size storage returned error: %1").arg(
                            QnLexical::serialized(errorCode)),
                        cl_logWARNING);

                    fail();
                    return;
                }

                errorCode = m_storage->updateFileInformation(
                    fileInfo.name, fileInfo.size, fileInfo.md5);

                if (errorCode != ErrorCode::noError)
                {
                    NX_LOG(
                        logMessage("During update storage returned error: %1").arg(
                            QnLexical::serialized(errorCode)),
                        cl_logWARNING);
                }
                NX_LOG(logMessage("Updated file info."), cl_logINFO);

                setState(State::foundFileInformation);
            }

            peerInfo.downloadedChunks = fileInfo.downloadedChunks;
            if (!addAvailableChunksInfo(fileInfo.downloadedChunks))
            {
                peerInfo.decreaseRank();
                return;
            }

            peerInfo.increaseRank();
            if (m_state == State::requestingAvailableChunks)
                setState(State::foundAvailableChunks);
        };

    for (const auto& peer: peers)
    {
        NX_LOG(
            logMessage("Requesting %1 from server %2.")
                .arg(subject)
                .arg(m_peerManager->peerString(peer)),
            cl_logDEBUG2);

        const auto handle = m_peerManager->requestFileInfo(peer, m_fileName, handleReply);
        if (handle > 0)
            m_peerByRequestHandle[handle] = peer;
    }

    if (m_peerByRequestHandle.isEmpty())
        waitForNextStep();
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

void Worker::requestChecksums()
{
    setState(State::requestingChecksums);

    const auto peers = selectPeersForOperation();
    if (peers.isEmpty())
    {
        waitForNextStep();
        return;
    }

    auto handleReply =
        [this](bool success, rest::Handle handle, const QVector<QByteArray>& checksums)
        {
            const auto peerId = m_peerByRequestHandle.take(handle);
            if (peerId.isNull())
                return;

            NX_LOG(
                logMessage("Got checksums from %1: %2")
                    .arg(m_peerManager->peerString(peerId))
                    .arg(statusString(success)),
                cl_logDEBUG2);

            auto exitGuard = QnRaiiGuard::createDestructible(
                [this, &success]()
                {
                    if (success)
                    {
                        cancelRequests();
                        waitForNextStep();
                        return;
                    }

                    waitForNextStep(0);
                });

            auto& peerInfo = m_peerInfoById[peerId];

            if (!success || checksums.isEmpty() || checksums.size() != m_availableChunks.size())
            {
                success = false;
                peerInfo.decreaseRank();
                return;
            }

            const auto errorCode =
                m_storage->setChunkChecksums(m_fileName, checksums);

            if (errorCode != ErrorCode::noError)
            {
                NX_LOG(
                    logMessage("Cannot set checksums: %1").arg(QnLexical::serialized(errorCode)),
                    cl_logWARNING);
                return;
            }

            NX_LOG(logMessage("Updated checksums."), cl_logDEBUG1);

            const auto& fileInfo = fileInformation();
            if (fileInfo.status != FileInformation::Status::downloading
                || fileInfo.downloadedChunks.count(true) == fileInfo.downloadedChunks.size())
            {
                fail();
                return;
            }

            setState(State::downloadingChunks);
        };

    for (const auto& peer: peers)
    {
        NX_LOG(
            logMessage("Requesting checksums from server %1.")
                .arg(m_peerManager->peerString(peer)),
            cl_logDEBUG2);

        const auto handle = m_peerManager->requestChecksums(peer, m_fileName, handleReply);
        if (handle > 0)
            m_peerByRequestHandle[handle] = peer;
    }

    if (m_peerByRequestHandle.isEmpty())
        waitForNextStep();
}

void Worker::downloadNextChunk()
{
    setState(State::downloadingChunks);

    const int chunkIndex = selectNextChunk();

    NX_ASSERT(chunkIndex >= 0);
    if (chunkIndex < 0)
        return;

    const auto& fileInfo = fileInformation();

    bool useInternet = false;
    auto peers = peersForChunk(chunkIndex);
    if (peers.isEmpty() && fileInfo.url.isValid())
    {
        useInternet = true;

        if (m_peerManager->hasInternetConnection(m_peerManager->selfId()))
            peers = {m_peerManager->selfId()};
        else
            peers = allPeersWithInternetConnection();
    }
    peers = selectPeersForOperation(1, peers);
    NX_ASSERT(!peers.isEmpty());
    NX_ASSERT(peers.size() == 1);
    if (peers.isEmpty())
    {
        waitForNextStep();
        return;
    }

    const auto& peerId = peers.first();

    auto handleReply =
        [this, chunkIndex](bool success, rest::Handle handle, const QByteArray& data)
        {
            const auto peerId = m_peerByRequestHandle.take(handle);
            if (peerId.isNull())
                return;

            NX_LOG(
                logMessage("Got chunk %2 from %1: %3")
                    .arg(m_peerManager->peerString(peerId))
                    .arg(chunkIndex)
                    .arg(statusString(success)),
                cl_logDEBUG2);

            auto exitGuard = QnRaiiGuard::createDestructible(
                [this, &success]()
                {
                    waitForNextStep(success ? 0 : -1);
                });

            auto& peerInfo = m_peerInfoById[peerId];

            if (!success)
            {
                peerInfo.decreaseRank();
                return;
            }

            const auto errorCode = m_storage->writeFileChunk(m_fileName, chunkIndex, data);
            if (errorCode != ErrorCode::noError)
            {
                NX_LOG(
                    logMessage("Cannot write chunk. Storage error: %1").arg(
                        QnLexical::serialized(errorCode)),
                    cl_logWARNING);

                // TODO: Implement error handling
                success = false;
                return;
            }
        };

    rest::Handle handle;

    if (useInternet)
    {
        NX_LOG(
            logMessage("Requesting chunk %1 from the Internet via %2...")
                .arg(chunkIndex).arg(m_peerManager->peerString(peerId)),
            cl_logDEBUG2);
        handle = m_peerManager->downloadChunkFromInternet(
            peerId, fileInfo.url, chunkIndex, fileInfo.chunkSize, handleReply);
    }
    else
    {
        NX_LOG(
            logMessage("Requesting chunk %1 from %2...")
                .arg(chunkIndex).arg(m_peerManager->peerString(peerId)),
            cl_logDEBUG2);
        handle = m_peerManager->downloadChunk(peerId, m_fileName, chunkIndex, handleReply);
    }

    if (handle < 0)
    {
        NX_LOG(
            logMessage("Cannot send request for chunk %1 to %2...")
                .arg(chunkIndex).arg(m_peerManager->peerString(peerId)),
            cl_logDEBUG2);

    }
    m_peerByRequestHandle[handle] = peerId;
}

void Worker::cancelRequests()
{
    auto peerByRequestHandle = m_peerByRequestHandle;
    m_peerByRequestHandle.clear();

    for (auto it = peerByRequestHandle.begin(); it != peerByRequestHandle.end(); ++it)
        m_peerManager->cancelRequest(it.value(), it.key());
}

void Worker::finish()
{
    setState(State::finished);
    NX_LOG(logMessage("Download finished."), cl_logINFO);
    emit finished(m_fileName);
}

void Worker::fail()
{
    setState(State::failed);
    NX_LOG(logMessage("Download failed."), cl_logERROR);
    emit failed(m_fileName);
}

QString Worker::logMessage(const char* message) const
{
    auto result = lit(" [%1] ").arg(m_fileName) + QString::fromLatin1(message);
    if (m_printSelfPeerInLogs)
    {
        result.prepend(
            lit("Worker [%1]").arg(m_peerManager->peerString(m_peerManager->selfId())));
    }
    else
    {
        result.prepend(NX_TYPE_NAME(typeid(*this)));
    }
    return result;
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

QList<QnUuid> Worker::allPeers() const
{
    return m_forcedPeers.isEmpty()
        ? m_peerManager->getAllPeers()
        : m_forcedPeers;
}

QList<QnUuid> Worker::allPeersWithInternetConnection() const
{
    QList<QnUuid> result;

    for (const auto& peer: allPeers())
    {
        if (m_peerManager->hasInternetConnection(peer))
            result.append(peer);
    }

    return result;
}

QList<QnUuid> Worker::selectPeersForOperation(
    int count, const QList<QnUuid>& referencePeers) const
{
    QList<QnUuid> result;

    auto exitLogGuard = QnRaiiGuard::createDestructible(
        [this, &result]
        {
            if (result.isEmpty())
                NX_LOG(logMessage("No peers selected."), cl_logDEBUG2);
        });

    auto peers = referencePeers.isEmpty() ? allPeers() : referencePeers;
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

    constexpr int kBigDistance = 8;

    for (const auto& peerId: peers)
    {
        const int rank = m_peerInfoById.value(peerId).rank;
        const int distance = m_peerManager->distanceTo(peerId);

        const int score = std::max(0, kBigDistance - distance) + rank * 2;
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
       1) One of the closest-guid peers: thus every server will try to use its
          guid neighbours which should lead to more uniform peers load in flat networks.
       2) At least one random peer which wasn't selected in the main selection: this is to avoid
          overloading peers which started file downloading and obviously contain chunks
          while other do not yet.
       3) The rest is the most scored peers having the closest guids.

       (1) and (2) are additionalPeers, so additionalPeers = 2.
       If there're not enought high-score peers then the result should contain
       random peers instead of them.
    */

    int additionalPeers = 2;
    const int highestScorePeersNeeded = count > additionalPeers
        ? count - additionalPeers
        : 1;

    QList<QnUuid> highestScorePeers;

    constexpr int kScoreThreshold = 3;
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
    result = takeClosestGuidPeers(highestScorePeers, highestScorePeersNeeded, selfId);

    if (result.size() == count)
        return result;

    additionalPeers = count - result.size();

    peers += highestScorePeers;
    std::sort(peers.begin(), peers.end());

    // Take (1) one closest-guid peer.
    result += takeClosestGuidPeers(peers, 1, selfId);
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
    const int randomChunk = rand() % chunksCount;

    int firstMetNotDownloadedChunk = -1;

    auto findChunk = [this, &fileInfo, &firstMetNotDownloadedChunk](int start, int end)
        {
            for (int i = start; i <= end; ++i)
            {
                if (fileInfo.downloadedChunks[i])
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

bool Worker::haveInternet() const
{
    if (m_peerManager->hasInternetConnection(m_peerManager->selfId()))
        return true;

    for (const auto& peerId: m_peerManager->getAllPeers())
    {
        if (m_peerManager->hasInternetConnection(peerId))
            return true;
    }

    return false;
}

void Worker::waitForNextStep(int delay)
{
    if (delay < 0)
        delay = kDefaultStepDelayMs;

    NX_LOG(logMessage("Start waiting for next step."), cl_logDEBUG2);

    if (delay == 0)
        nextStep();
    else
        m_stepDelayTimer->start(delay);
}

void Worker::increasePeerRank(const QnUuid& peerId, int value)
{
    m_peerInfoById[peerId].increaseRank(value);
}

void Worker::decreasePeerRank(const QnUuid& peerId, int value)
{
    m_peerInfoById[peerId].decreaseRank(value);
}

void Worker::setPrintSelfPeerInLogs()
{
    m_printSelfPeerInLogs = true;
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

} // namespace distributed_file_downloader
} // namespace common
} // namespace vms
} // namespace nx
