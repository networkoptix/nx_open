// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "worker.h"

#include <chrono>

#include <QtCore/QTimer>

#include <nx/utils/guarded_callback.h>
#include <nx/utils/log/log.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/random.h>

#include "storage.h"
#include "abstract_peer_manager.h"

namespace {

using namespace nx::vms::common::p2p::downloader;

constexpr int kMaxSimultaneousDownloads = 5;
constexpr milliseconds kDefaultStepDelay = 15s;

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
        case Worker::State::foundAvailableChunks:
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
struct RequestContext
{
    AbstractPeerManager::RequestContextPtr<T> context;
    UserData data;

    RequestContext(const UserData& data, AbstractPeerManager::RequestContextPtr<T> context):
        context(std::move(context)),
        data(data)
    {
    }

    static void processRequests(
        Worker* worker,
        std::vector<RequestContext>& contexts,
        std::function<bool(const UserData&, const std::optional<T>&)> handleReply,
        bool cancelUnprocessedRequests = true)
    {
        while (!contexts.empty() && !worker->needToStop())
        {
            auto it = contexts.begin();
            while (it != contexts.end() && !worker->needToStop())
            {
                if (it->context->future.wait_for(30ms) == std::future_status::ready)
                    break;
                ++it;
            }

            if (worker->needToStop())
                break;

            if (it == contexts.end())
                continue;

            const bool stopProcessing = handleReply(it->data, it->context->future.get());
            contexts.erase(it);

            if (stopProcessing)
                break;
        }

        if (cancelUnprocessedRequests || worker->needToStop())
        {
            if (!contexts.empty())
                NX_VERBOSE(worker->logTag(), "Cancelling %1 requests", contexts.size());

            for (auto& context: contexts)
                context.context->cancel();

            contexts.clear();
        }
    }
};

} // namespace

namespace nx::vms::common::p2p::downloader {

Worker::Worker(
    const QString& fileName,
    Storage* storage,
    const QList<AbstractPeerManager*>& peerManagers,
    const nx::Uuid& selfId)
    :
    m_selfId(selfId),
    m_storage(storage),
    m_peerManagers(peerManagers),
    m_fileName(fileName)
{
    m_logTag = nx::utils::log::Tag(QStringLiteral("%1 [%2]").arg(nx::toString(this), m_fileName));

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

    m_stallDetectionTimer.restart();
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
    if (!m_needStop)
    {
        m_sleeping = true;
        m_sleepCondition.wait_for(lk, delay(), [this]() { return !m_sleeping; });
    }
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

bool Worker::haveChunksToDownload()
{
    NX_VERBOSE(m_logTag, "Checking if we have chunks to download...");

    const auto& fileInfo = fileInformation();
    if (!fileInfo.isValid())
        return false;

    for (auto it = m_peerInfoByPeer.begin(); it != m_peerInfoByPeer.end(); ++it)
    {
        if (it->isInternet && !it->isBanned())
        {
            NX_VERBOSE(m_logTag, "Not banned internet peer %1 is found.", it.key());
            return true;
        }
    }

    const int chunksCount = fileInfo.downloadedChunks.size();
    if (m_availableChunks.size() != chunksCount)
        return false;

    for (int i = 0; i < chunksCount; ++i)
    {
        if (m_availableChunks[i] && !fileInfo.downloadedChunks[i])
        {
            NX_VERBOSE(m_logTag, "Chunk %1 is available to download", i);
            return true;
        }
    }

    NX_VERBOSE(m_logTag, "No chunks are available to download.");
    return false;
}

utils::log::Tag Worker::logTag() const
{
    return m_logTag;
}

bool Worker::isStalled() const
{
    return m_stalled;
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

        checkStalled();

        NX_VERBOSE(m_logTag, "doWork(): Start iteration in state %1", m_state);

        switch (m_state)
        {
            case State::initial:
                if (fileInfo.status == FileInformation::Status::downloaded)
                    finish();
                else if (fileInfo.size < 0 || fileInfo.md5.isEmpty())
                    requestFileInformation();
                else if (fileInfo.status == FileInformation::Status::corrupted)
                    reDownload();
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
                    reDownload();
                    break;

                case FileInformation::Status::downloading:
                    if (!haveChunksToDownload())
                        requestAvailableChunks();
                    else if (needToFindBetterPeersForDownload())
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
    constexpr int kMaxPeersToCheckAtOnce = 10;

    NX_VERBOSE(m_logTag, "Trying to get %1.", requestSubjectString(m_state));

    const auto& sleepOrEnterDownloadingChunks =
        [this]()
        {
            if (m_state == State::requestingAvailableChunks && haveChunksToDownload())
                setState(State::foundAvailableChunks);
            else
                sleep();
        };

    const QSet<Peer>& peers = getPeersToCheckInfo();
    if (peers.isEmpty())
    {
        sleepOrEnterDownloadingChunks();
        if (m_state != State::foundAvailableChunks)
            reviveBannedPeers();
        return;
    }

    const nx::utils::Url& url = fileInformation().url;

    using Context = RequestContext<FileInformation, Peer>;
    std::vector<Context> contexts;

    int requestCount = 0;
    for (const auto& peer: peers)
    {
        if (needToStop())
            break;

        NX_VERBOSE(m_logTag, "Requesting %1 from server %2.",
            requestSubjectString(m_state), peer);

        auto context = peer.manager->requestFileInfo(peer.id, m_fileName, url);
        if (context && context->isValid())
        {
            contexts.emplace_back(peer, std::move(context));
            ++requestCount;
        }
        else
        {
            decreasePeerRank(peer);
        }

        if (requestCount >= kMaxPeersToCheckAtOnce)
            break;
    }

    if (contexts.empty())
    {
        sleepOrEnterDownloadingChunks();
        return;
    }

    Context::processRequests(this, contexts,
        [this](const Peer& peer, const std::optional<FileInformation>& fileInformation)
        {
            handleFileInformationReply(peer, fileInformation);
            return false;
        });

    if (m_state == State::requestingAvailableChunks && !haveChunksToDownload())
        reviveBannedPeers();

    if (m_state == State::requestingFileInformation || m_state == State::requestingAvailableChunks)
        sleepOrEnterDownloadingChunks();
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

    const int oldAvailableChunks = m_availableChunks.count(true);
    peerInfo.downloadedChunks = fileInfo->downloadedChunks;
    const int newAvailableChunks = updateAvailableChunks();

    if (newAvailableChunks > oldAvailableChunks || peerInfo.isInternet)
        increasePeerRank(peer);

    if (m_state == State::requestingAvailableChunks && haveChunksToDownload())
        setState(State::foundAvailableChunks);
}

void Worker::requestChecksums()
{
    constexpr int kMaxPeersToCheckAtOnce = 3;

    NX_VERBOSE(m_logTag, "requestChecksums()");

    setState(State::requestingChecksums);

    QList<Peer> peers = getPeersToGetCheksums();
    if (peers.isEmpty())
    {
        NX_VERBOSE(m_logTag, "requestChecksums(): No peers selected. Exiting.");
        reviveBannedPeers();
        m_retryingStep = true;
        sleep();
        return;
    }

    while (!peers.isEmpty() && !needToStop())
    {
        using Context = RequestContext<QVector<QByteArray>, Peer>;
        std::vector<Context> contexts;

        while (!peers.isEmpty() && contexts.size() < kMaxPeersToCheckAtOnce && !needToStop())
        {
            const Peer& peer = peers.takeFirst();

            NX_VERBOSE(m_logTag, "requestChecksums(): Requesting %1 from %2.",
                requestSubjectString(State::requestingChecksums), peer);

            auto context = peer.manager->requestChecksums(peer.id, m_fileName);
            if (context && context->isValid())
                contexts.emplace_back(peer, std::move(context));
            else
                decreasePeerRank(peer);
        }

        if (contexts.empty())
            break;

        Context::processRequests(this, contexts,
            [this](const Worker::Peer& peer, const std::optional<QVector<QByteArray>>& checksums)
            {
                handleChecksumsReply(peer, checksums);
                return m_state != State::requestingChecksums;
            });
    }

    if (m_state == State::requestingChecksums)
    {
        if (m_retryingStep)
        {
            m_retryingStep = false;

            m_storage->clearFile(m_fileName);
            updateAvailableChunks();
            setState(State::downloadingChunks);

            return;
        }

        sleep();
        return;
    }

    m_retryingStep = false;
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

    const auto resultCode = m_storage->setChunkChecksums(m_fileName, *checksums);

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
    constexpr int kSubsequentChunksToDownload = 15;

    NX_VERBOSE(m_logTag, "downloadChunks()");

    setState(State::downloadingChunks);

    const auto& fileInfo = fileInformation();

    struct ContextData
    {
        Peer peer;
        int chunkIndex;
        time_point<steady_clock> requestTime;
    };

    using Context = RequestContext<nx::Buffer, ContextData>;
    std::vector<Context> contexts;

    int chunksLeft = kSubsequentChunksToDownload;
    bool chunkRequested = false;

    QSet<int> downloadingChunks;
    QSet<Peer> busyPeers;

    while (chunksLeft > 0 && !needToStop())
    {
        for (int i = 0;
            contexts.size() < kMaxSimultaneousDownloads && i < kMaxSimultaneousDownloads
                && chunksLeft > 0 && !needToStop();
            ++i)
        {
            const int chunk = selectNextChunk(downloadingChunks);
            if (chunk < 0)
                break;

            const Peer& peer = selectPeerForChunk(chunk, busyPeers);
            if (peer.isNull())
            {
                NX_WARNING(m_logTag, "No peers are selected to download chunk %1.", chunk);
                continue;
            }

            const PeerInformation& peerInfo = m_peerInfoByPeer.value(peer);

            NX_VERBOSE(m_logTag,
                "Selected peer %1 for chunk %2, rank: %3, chunk download time: %4",
                peer, chunk, peerInfo.rank, peerInfo.averageChunkDownloadTime);

            auto context = peer.manager->downloadChunk(
                peer.id, m_fileName, fileInfo.url, chunk, (int) fileInfo.chunkSize, fileInfo.size);
            if (context && context->isValid())
            {
                chunkRequested = true;
                contexts.emplace_back(
                    ContextData{peer, chunk, steady_clock::now()}, std::move(context));
                downloadingChunks.insert(chunk);
                busyPeers.insert(peer);
                --chunksLeft;
            }
            else
            {
                decreasePeerRank(peer);
            }
        }

        if (contexts.empty())
            break;

        if (chunksLeft == 0 && !needToFindBetterPeersForDownload())
        {
            // We'll get here with no change in this case. But this way we'll not wait when the
            // latest single chunk is downloaded. Instead we'll keep operating with maximum
            // concurrent chunks.
            chunksLeft = kSubsequentChunksToDownload;
        }

        Context::processRequests(this, contexts,
            [&, this](const ContextData& ctx, const std::optional<nx::Buffer>& data)
            {
                PeerInformation& info = m_peerInfoByPeer[ctx.peer];

                if (data)
                {
                    info.lastSuccessfulRequestTime = ctx.requestTime;

                    const bool lastChunk = ctx.chunkIndex == m_availableChunks.size() - 1;
                    if (!lastChunk)
                    {
                        // Last chunk is usually smaller than others, so skip it.
                        info.recordChunkDownloadTime(
                            duration_cast<milliseconds>(steady_clock::now() - ctx.requestTime));
                    }
                }

                const bool decreaseRankOnFailure =
                    info.lastSuccessfulRequestTime < ctx.requestTime;

                handleDownloadChunkReply(ctx.peer, ctx.chunkIndex, data, decreaseRankOnFailure);

                busyPeers.remove(ctx.peer);
                downloadingChunks.remove(ctx.chunkIndex);

                return chunksLeft > 0;
            },
            false);
    }

    if (!chunkRequested)
    {
        if (fileInformation().status != FileInformation::Status::downloading)
        {
            // This may happen if file is being downloaded and uploaded at the same time.
            return;
        }

        reviveBannedPeers();
        sleep();
        return;
    }
}

void Worker::handleDownloadChunkReply(
    const Peer& peer,
    int chunkIndex,
    const std::optional<nx::Buffer>& data,
    bool decreaseRankOnFailure)
{
    NX_VERBOSE(m_logTag, "Got chunk %1 from %2: %3",
        chunkIndex, peer, statusString(data.has_value()));

    if (!data)
    {
        emit chunkDownloadFailed(m_fileName);
        if (decreaseRankOnFailure)
            decreasePeerRank(peer);
        return;
    }

    const auto resultCode =
        m_storage->writeFileChunk(m_fileName, chunkIndex, data->toRawByteArray());
    if (resultCode != ResultCode::ok)
    {
        NX_WARNING(m_logTag, "Cannot write chunk %1. Storage error: %2", chunkIndex, resultCode);
        ++m_subsequentWriteErrors;

        constexpr int kMaxSubsequentWriteErrors = 10;
        if (m_subsequentWriteErrors >= kMaxSubsequentWriteErrors)
        {
            NX_ERROR(
                m_logTag, "There are too many chunk write errors. Cannot continue downloading.");
            finish(State::failed);
            m_subsequentWriteErrors = 0;
            return;
        }
    }
    else
    {
        m_subsequentWriteErrors = 0;
    }

    markActive();
}

void Worker::finish(State state)
{
    setState(state);
    NX_INFO(m_logTag, "Download finished: %1.", state);
    emit finished(m_fileName);
}

int Worker::updateAvailableChunks()
{
    NX_VERBOSE(m_logTag, "Updating available chunks...");

    m_availableChunks = fileInformation().downloadedChunks;
    int availableChunksCount = m_availableChunks.count(true);

    for (const PeerInformation& peer: m_peerInfoByPeer)
    {
        if (peer.isBanned())
            continue;

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

    return availableChunksCount;
}

FileInformation Worker::fileInformation() const
{
    const auto& fileInfo = m_storage->fileInformation(m_fileName);
    NX_ASSERT(fileInfo.isValid());

    if (fileInfo.downloadedChunks.isEmpty() && !m_availableChunks.isEmpty())
        NX_ASSERT(fileInfo.downloadedChunks.size() == m_availableChunks.size());

    return fileInfo;
}

QSet<Worker::Peer> Worker::getPeersToCheckInfo() const
{
    QSet<Peer> result;

    for (AbstractPeerManager* manager: m_peerManagers)
    {
        if (!manager->capabilities.testFlag(AbstractPeerManager::Capability::FileInfo))
            continue;

        for (const nx::Uuid& peerId: manager->peers())
        {
            const Peer peer{peerId, manager};

            const auto it = m_peerInfoByPeer.find(peer);
            if (it == m_peerInfoByPeer.end())
            {
                result.insert(peer);
                continue;
            }

            const PeerInformation& info = *it;
            if (info.isBanned())
                continue;

            if (info.isInternet)
                continue;

            if (!info.downloadedChunks.isEmpty()
                && info.downloadedChunks.count(true) == info.downloadedChunks.size())
            {
                continue;
            }

            result.insert(peer);
        }
    }

    return result;
}

QList<Worker::Peer> Worker::getPeersToGetCheksums() const
{
    QList<Peer> preferredPeers;
    QList<Peer> otherPeers;

    for (AbstractPeerManager* manager: m_peerManagers)
    {
        if (!manager->capabilities.testFlag(AbstractPeerManager::Capability::Checksums))
            continue;

        for (const nx::Uuid& peerId: manager->peers())
        {
            const Peer peer{peerId, manager};

            const auto it = m_peerInfoByPeer.find(peer);
            if (it == m_peerInfoByPeer.end())
            {
                otherPeers.append(peer);
                continue;
            }

            if (it->isBanned())
                continue;

            preferredPeers.append(peer);
        }
    }

    return preferredPeers + otherPeers;
}

Worker::Peer Worker::selectPeerForChunk(int chunk, const QSet<Worker::Peer>& busyPeers) const
{
    const auto findBestPeer =
        [this, chunk, busyPeers](bool freeOnly)
        {
            Peer bestPeer;
            milliseconds bestTime = milliseconds::max();

            for (auto it = m_peerInfoByPeer.begin(); it != m_peerInfoByPeer.end(); ++it)
            {
                if (it->isBanned())
                    continue;

                if (freeOnly && busyPeers.contains(it.key()))
                    continue;

                if ((it->isInternet || it->hasChunk(chunk))
                    && it->averageChunkDownloadTime < bestTime)
                {
                    bestPeer = it.key();
                    bestTime = it->averageChunkDownloadTime;
                }
            }

            return bestPeer;
        };

    if (const Peer& peer = findBestPeer(true); !peer.isNull())
        return peer;

    return findBestPeer(false);
}

void Worker::reviveBannedPeers()
{
    NX_DEBUG(m_logTag, "reviveBannedPeers()");
    for (auto& peerInfo: m_peerInfoByPeer)
    {
        if (peerInfo.isBanned())
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

bool Worker::needToFindBetterPeersForDownload() const
{
    NX_VERBOSE(m_logTag, "Checking if need to find better peers...");

    constexpr int kGoodDownloadSpeed = 5; //< Megabytes per second.
    const milliseconds goodChunkDownloadTime{
        (int) (fileInformation().chunkSize / kGoodDownloadSpeed)}; //< Milliseconds.

    int fastPeersCount = 0;

    for (const PeerInformation& info: m_peerInfoByPeer)
    {
        if (info.isBanned() || info.averageChunkDownloadTime == 0ms)
            continue;

        if (info.averageChunkDownloadTime <= goodChunkDownloadTime)
            ++fastPeersCount;
    }

    if (fastPeersCount > kMaxSimultaneousDownloads)
    {
        NX_VERBOSE(m_logTag, "Don't need to find better peers. Found %1 fast peers.",
            fastPeersCount);
        return false;
    }

    int peersCount = 0;
    for (AbstractPeerManager* peerManager: m_peerManagers)
        peersCount += peerManager->peers().size();

    int checkedPeersCount = 0;
    for (const PeerInformation& info: m_peerInfoByPeer)
    {
        if (!info.downloadedChunks.isEmpty() || info.isInternet)
            ++checkedPeersCount;
    }

    const bool result = m_peerInfoByPeer.size() < peersCount;
    if (result)
    {
        NX_VERBOSE(m_logTag, "Need to find better peers. Fast peers: %1, checked: %2, total: %3",
            fastPeersCount, checkedPeersCount, peersCount);
    }
    else
    {
        NX_VERBOSE(m_logTag, "Don't need to find better peers. Checked all %1 peers.", peersCount);
    }

    return result;
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
    if (peerInfo.isBanned())
    {
        // The peer probably lost the file. Need to re-check.
        peerInfo.isInternet = false;
        peerInfo.downloadedChunks.clear();
        updateAvailableChunks();
    }
    NX_VERBOSE(m_logTag, "Decreasing rank of %1: %2", peer, peerInfo.rank);
}

QList<AbstractPeerManager*> Worker::peerManagers() const
{
    return m_peerManagers;
}

void Worker::markActive()
{
    NX_VERBOSE(m_logTag, "Marking the download as active...");
    m_stallDetectionTimer.restart();
    checkStalled();
}

void Worker::checkStalled()
{
    constexpr milliseconds kStallDetectionTimeout = 2min;

    const bool stalled = m_stallDetectionTimer.hasExpired(kStallDetectionTimeout.count());

    if (m_stalled == stalled)
        return;

    m_stalled = stalled;

    if (stalled)
        NX_WARNING(m_logTag, "Download is stalled.");
    else
        NX_INFO(m_logTag, "Download is no longer stalled.");

    emit stalledChanged(stalled);
}

void Worker::reDownload()
{
    constexpr int kMaxDownloadRetries = 2;

    if (m_downloadRetriesCount >= kMaxDownloadRetries)
    {
        finish(State::failed);
        return;
    }

    ++m_downloadRetriesCount;
    requestChecksums();
}

void Worker::PeerInformation::increaseRank(int value)
{
    rank = qBound(kMinAutoRank, rank + value, kMaxAutoRank);
}

void Worker::PeerInformation::decreaseRank(int value)
{
    increaseRank(-value);
}

void Worker::PeerInformation::recordChunkDownloadTime(milliseconds time)
{
    constexpr int kMaxTimesToStore = 5;

    latestChunksDownloadTime.append(time);
    while (latestChunksDownloadTime.size() > kMaxTimesToStore)
        latestChunksDownloadTime.removeFirst();

    averageChunkDownloadTime =
        std::accumulate(latestChunksDownloadTime.begin(), latestChunksDownloadTime.end(), 0ms)
            / latestChunksDownloadTime.size();
}

bool Worker::PeerInformation::hasChunk(int chunk) const
{
    return isInternet || (chunk < downloadedChunks.size() && downloadedChunks[chunk]);
}

} // namespace nx::vms::common::p2p::downloader
