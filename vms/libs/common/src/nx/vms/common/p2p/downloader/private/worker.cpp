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
constexpr int kSubsequentChunksToDownload = 10;
static const int kMaxSimultaneousDownloads = 5;
constexpr milliseconds kDefaultStepDelay = 1min;
constexpr int kMaxAutoRank = 5;
constexpr int kMinAutoRank = -1;

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

template<typename T, typename UserData>
struct RequestContext: AbstractPeerManager::RequestContext<T>
{
    UserData data;
    int chunkIndex = -1;

    RequestContext(const UserData& data, AbstractPeerManager::RequestContext<T>&& context):
        AbstractPeerManager::RequestContext<T>(std::move(context)),
        data(data)
    {
    }

    static void processRequests(
        Worker* worker,
        std::vector<RequestContext>& contexts,
        std::function<bool(const UserData&, const std::optional<T>&)> handleReply)
    {
        while (!contexts.empty() && !worker->needToStop())
        {
            auto it = contexts.begin();
            while (it != contexts.end() && !worker->needToStop())
            {
                if (it->future.wait_for(2s) == std::future_status::ready)
                    break;
                ++it;
            }

            if (worker->needToStop())
                break;

            if (it == contexts.end())
                continue;

            const bool stopProcessing = handleReply(it->data, it->future.get());
            contexts.erase(it);

            if (stopProcessing)
                break;
        }

        if (!contexts.empty())
            NX_VERBOSE(worker->logTag(), "Cancelling %1 requests", contexts.size());

        for (auto& context: contexts)
            context.cancel();

        contexts.clear();
    }
};

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

    m_started = true;
    doWork();
}

void Worker::pleaseStop()
{
    NX_INFO(m_logTag, "Stopping...");
    m_needStop = true;
    wake();
}

void Worker::sleep()
{
    NX_VERBOSE(m_logTag, "Start waiting.");

    std::unique_lock<std::mutex> lk(m_sleepMutex);
    m_sleeping = true;
    m_sleepCondition.wait_for(lk, delay(), [this]() { return !m_sleeping; });
    m_sleeping = false;

    NX_VERBOSE(m_logTag, "Waking...");
}

void Worker::wake()
{
    std::lock_guard<std::mutex> lk(m_sleepMutex);
    if (!m_sleeping)
        return;

    NX_VERBOSE(m_logTag, "wake()");
    m_sleeping = false;
    m_sleepCondition.notify_one();
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
        if (m_availableChunks[i] && !fileInfo.downloadedChunks[i])
            return true;
    }
    return false;
}

utils::log::Tag Worker::logTag() const
{
    return m_logTag;
}

void Worker::setState(State state)
{
    if (m_state == state)
        return;

    NX_VERBOSE(m_logTag, "Entering state %1...", state);
    m_state = state;
    emit stateChanged(state);
}

void Worker::doWork()
{
    while (true)
    {
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
                else if (!haveChunksToDownload())
                    requestAvailableChunks();
                else if (fileInfo.status == FileInformation::Status::downloading)
                    downloadChunks();
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
                    downloadChunks();
                }
                else
                {
                    setState(State::requestingAvailableChunks);
                    sleep();
                }
                break;

            case State::foundAvailableChunks:
                downloadChunks();
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
                    if (!haveChunksToDownload())
                        requestAvailableChunks();
                    else if (needToFindBetterPeers())
                        requestAvailableChunks();
                    else
                        downloadChunks();
                    break;

                default:
                    NX_ASSERT(false, "Should never get here.");
                }
                break;

            case State::finished:
            case State::failed:
                pleaseStop();
                return;

            default:
                NX_ASSERT(false, "Should never get here.");
                break;
        }
    }
}

milliseconds Worker::delay() const
{
    return kDefaultStepDelay;
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

void Worker::requestFileInformationInternal()
{
    NX_VERBOSE(m_logTag, "Trying to get %1.", requestSubjectString(m_state));

    const QList<Peer>& peers = selectPeersForOperation(AbstractPeerManager::FileInfo);
    if (peers.isEmpty())
    {
        revivePeersWithMinimalRank();
        sleep();
        return;
    }

    const nx::utils::Url& url = fileInformation().url;

    using Context = RequestContext<FileInformation, Peer>;
    std::vector<Context> contexts;

    for (const auto& peer: peers)
    {
        NX_VERBOSE(m_logTag, "Requesting %1 from server %2.",
            requestSubjectString(m_state), peer);

        auto context = peer.manager->requestFileInfo(peer.id, m_fileName, url);
        if (context.isValid())
            contexts.emplace_back(peer, std::move(context));
        else
            decreasePeerRank(peer);
    }

    if (contexts.empty())
    {
        sleep();
        return;
    }

    Context::processRequests(this, contexts,
        [this](const Peer& peer, const std::optional<FileInformation>& fileInformation)
        {
            handleFileInformationReply(peer, fileInformation);
            return false;
        });

    if (m_state == State::requestingFileInformation || m_state == State::requestingAvailableChunks)
        sleep();
}

void Worker::handleFileInformationReply(
    const Peer& peer, const std::optional<FileInformation>& fileInfo)
{
    NX_VERBOSE(m_logTag, "handleFileInformationReply(): Got %3 reply from %1: %2",
        peer,
        statusString(fileInfo.has_value()),
        requestSubjectString(m_state));

    auto& peerInfo = m_peerInfoByPeer[peer];

    if (!fileInfo)
    {
        decreasePeerRank(peer);
        return;
    }

    peerInfo.isInternet = fileInfo->downloadedChunks.isEmpty();

    if (m_state == State::requestingFileInformation)
    {
        if (fileInfo->size < 0 || fileInfo->md5.isEmpty() || fileInfo->chunkSize <= 0)
        {
            if (!peerInfo.isInternet)
            {
                decreasePeerRank(peer);
                return;
            }
        }

        auto resultCode = m_storage->setChunkSize(fileInfo->name, fileInfo->chunkSize);
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
            fileInfo->name, fileInfo->size, fileInfo->md5);

        m_availableChunks.resize(
            Storage::calculateChunkCount(fileInfo->size, fileInfo->chunkSize));

        if (resultCode != ResultCode::ok)
        {
            NX_WARNING(m_logTag,
                "handleFileInformationReply(): During update storage returned error: %1",
                resultCode);
        }

        NX_INFO(m_logTag, "handleFileInformationReply(): Updated file info.");

        setState(State::foundFileInformation);
    }

    peerInfo.downloadedChunks = fileInfo->downloadedChunks;
    updateAvailableChunks();

    increasePeerRank(peer);

    if (m_state == State::requestingAvailableChunks)
    {
        if (haveChunksToDownload())
            setState(State::foundAvailableChunks);
        else
            sleep();
    }
}

void Worker::requestChecksums()
{
    NX_VERBOSE(m_logTag, "requestChecksums()");

    setState(State::requestingChecksums);

    const auto peers = selectPeersForOperation(AbstractPeerManager::Checksums);
    if (peers.isEmpty())
    {
        NX_VERBOSE(m_logTag, "requestChecksums(): No peers selected. Exiting.");
        revivePeersWithMinimalRank();
        sleep();
        return;
    }

    using Context = RequestContext<QVector<QByteArray>, Peer>;
    std::vector<Context> contexts;

    for (const auto& peer: peers)
    {
        NX_VERBOSE(m_logTag, "requestChecksums(): Requesting %1 from server %2.",
            requestSubjectString(State::requestingChecksums), peer);

        auto context = peer.manager->requestChecksums(peer.id, m_fileName);
        if (context.isValid())
            contexts.emplace_back(peer, std::move(context));
        else
            decreasePeerRank(peer);
    }

    if (contexts.empty())
    {
        sleep();
        return;
    }

    Context::processRequests(this, contexts,
        [this](const Worker::Peer& peer, const std::optional<QVector<QByteArray>>& checksums)
        {
            handleChecksumsReply(peer, checksums);
            return m_state != State::requestingChecksums;
        });

    if (m_state == State::requestingChecksums)
        sleep();
}

void Worker::handleChecksumsReply(
    const Peer& peer, const std::optional<QVector<QByteArray>>& checksums)
{
    NX_VERBOSE(m_logTag, "handleChecksumsReply(): Got %1 from %2: %3",
        requestSubjectString(State::requestingChecksums),
        peer,
        statusString(checksums.has_value()));

    if (!checksums || checksums->isEmpty() || checksums->size() != m_availableChunks.size())
    {
        NX_VERBOSE(m_logTag, "handleChecksumsReply(): Got invalid reply. Exiting.");
        decreasePeerRank(peer);
        return;
    }

    const auto resultCode = m_storage->setChunkChecksums(m_fileName, checksums.value());

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

void Worker::downloadChunks()
{
    NX_VERBOSE(m_logTag, "downloadChunks()");

    setState(State::downloadingChunks);

    const auto& fileInfo = fileInformation();

    using Context = RequestContext<QByteArray, std::pair<Peer, int>>;
    std::vector<Context> contexts;

    int chunksLeft = kSubsequentChunksToDownload;
    bool chunkRequested = false;

    while (chunksLeft > 0)
    {
        QSet<int> downloadingChunks;
        QSet<Peer> busyPeers;

        for (int i = 0; i < kMaxSimultaneousDownloads && chunksLeft > 0; ++i, --chunksLeft)
        {
            const int chunkIndex = selectNextChunk(downloadingChunks);
            if (chunkIndex < 0)
                break;

            const QSet<Peer>& availablePeers = peersForChunk(chunkIndex);
            if (availablePeers.isEmpty())
            {
                // A chunk to be downloaded from Internet is selected, but we don't have Internet
                // peers.
                break;
            }

            // Trying to aoid busy peers to balance network load. But this is not absolutely
            // necessary.
            QSet<Peer> peers = availablePeers - busyPeers;
            if (peers.isEmpty())
                peers = availablePeers;

            const QList<Peer> selectedPeers =
                selectPeersForOperation(AbstractPeerManager::DownloadChunk, 1, peers.toList());
            if (selectedPeers.isEmpty())
            {
                NX_WARNING(m_logTag, "No peers are selected to download chunk %1.", chunkIndex);
                continue;
            }

            const auto& peer = selectedPeers.first();
            NX_VERBOSE(m_logTag, "Selected peer %1, rank: %2, distance: %3",
                peer,
                m_peerInfoByPeer[peer].rank,
                peer.manager->distanceTo(peer.id));

            NX_VERBOSE(m_logTag, "Requesting chunk %1 from %2...", chunkIndex, peer);

            auto context = peer.manager->downloadChunk(
                peer.id, m_fileName, fileInfo.url, chunkIndex, (int) fileInfo.chunkSize);
            if (context.isValid())
            {
                chunkRequested = true;
                contexts.emplace_back(std::make_pair(peer, chunkIndex), std::move(context));
                downloadingChunks.insert(chunkIndex);
            }
            else
            {
                decreasePeerRank(peer);
            }
        }

        if (contexts.empty())
            break;

        Context::processRequests(this, contexts,
            [this](const std::pair<Worker::Peer, int>& ctx, const std::optional<QByteArray>& data)
            {
                handleDownloadChunkReply(ctx.first, ctx.second, data);
                return false;
            });
    }

    if (!chunkRequested)
    {
        if (fileInformation().status != FileInformation::Status::downloading)
        {
            // This may happen if file is being downloaded and uploaded at the same time.
            return;
        }

        revivePeersWithMinimalRank();
        sleep();
        return;
    }
}

void Worker::handleDownloadChunkReply(
    const Peer& peer, int chunkIndex, const std::optional<QByteArray>& data)
{
    NX_VERBOSE(m_logTag, "Got chunk %1 from %2: %3",
        chunkIndex, peer, statusString(data.has_value()));

    if (!data)
    {
        emit chunkDownloadFailed(m_fileName);
        decreasePeerRank(peer);
        return;
    }

    const auto resultCode = m_storage->writeFileChunk(m_fileName, chunkIndex, data.value());
    if (resultCode != ResultCode::ok)
    {
        NX_WARNING(m_logTag, "Cannot write chunk %1. Storage error: %2", chunkIndex, resultCode);

        // TODO: Implement error handling.
        emit chunkDownloadFailed(m_fileName);
        return;
    }
}

void Worker::finish(State state)
{
    setState(state);
    NX_INFO(m_logTag, "Download finished: %1.", state);
    emit finished(m_fileName);
}

void Worker::updateAvailableChunks()
{
    NX_VERBOSE(m_logTag, "Updating available chunks...");

    m_availableChunks = fileInformation().downloadedChunks;
    int availableChunksCount = m_availableChunks.count(true);

    for (const PeerInformation& peer: m_peerInfoByPeer)
    {
        if (availableChunksCount == m_availableChunks.size())
            break;

        if (peer.isInternet || peer.downloadedChunks.isEmpty())
            continue;

        for (int i = 0; i < m_availableChunks.size(); ++i)
        {
            if (peer.downloadedChunks[i] && !m_availableChunks[i])
            {
                ++availableChunksCount;
                m_availableChunks.setBit(i);
            }
        }
    }

    NX_VERBOSE(m_logTag, "Chunks available: %1/%2",
        availableChunksCount, m_availableChunks.size());
}

QSet<Worker::Peer> Worker::peersForChunk(int chunkIndex) const
{
    QSet<Peer> result;

    for (auto it = m_peerInfoByPeer.begin(); it != m_peerInfoByPeer.end(); ++it)
    {
        if (it->isInternet
            || (it->downloadedChunks.size() > chunkIndex && it->downloadedChunks[chunkIndex]))
        {
            result.insert(it.key());
        }
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

int Worker::selectNextChunk(const QSet<int>& ignoredChunks) const
{
    const auto& fileInfo = fileInformation();
    if (!fileInfo.isValid())
        return -1;

    const int chunksCount = fileInfo.downloadedChunks.size();
    const int randomChunk = utils::random::number(0, chunksCount - 1);

    int firstMetNotDownloadedChunk = -1;

    auto findChunk =
        [&](int start, int end)
        {
            for (int i = start; i <= end; ++i)
            {
                if (fileInfo.downloadedChunks[i] || ignoredChunks.contains(i))
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
