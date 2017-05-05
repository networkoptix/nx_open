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
constexpr int kMaxAutoRank = 4;
constexpr int kMinAutoRank = -1;
constexpr int kDefaultRank = 0;

QString statusString(bool success)
{
    return success ? lit("OK") : lit("FAIL");
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

    NX_LOGX(logMessage("Created."), cl_logINFO);
}

Worker::~Worker()
{
}

void Worker::start()
{
    if (m_started)
        return;

    NX_LOGX(logMessage("Starting..."), cl_logINFO);

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

    NX_LOGX(logMessage("Entering state %1...").arg(QnLexical::serialized(state)), cl_logDEBUG1);
    m_state = state;
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
                    downloadNextChunk();
                    break;
                default:
                    NX_ASSERT(false, "Should never get here.");
            }
            break;

        case State::finished:
            break;

        default:
            NX_ASSERT(false, "Should never get here.");
            waitForNextStep();
            break;
    }
}

void Worker::requestFileInformationInternal()
{
    NX_LOGX(logMessage("Trying to get file info."), cl_logDEBUG2);

    const auto peers = selectPeersForOperation();
    if (peers.isEmpty())
    {
        waitForNextStep();
        return;
    }

    auto handleReply =
        [this](bool success, rest::Handle handle, const FileInformation& fileInfo)
        {
            const auto peerId = m_peerByRequestHandle.take(handle);
            if (peerId.isNull())
                return;

            NX_LOGX(
                logMessage("Got file info reply from %1: %2")
                    .arg(m_peerManager->peerString(peerId))
                    .arg(statusString(success)),
                cl_logDEBUG2);

            auto exitGuard = QnRaiiGuard::createDestructible(
                [this]()
                {
                    if (!m_peerByRequestHandle.isEmpty())
                        return;

                    if (m_state == State::requestingFileInformation
                        || m_state == State::requestingAvailableChunks)
                    {
                        waitForNextStep();
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
                && fileInfo.size >= 0 && !fileInfo.md5.isEmpty())
            {
                const auto errorCode = m_storage->updateFileInformation(
                    fileInfo.name, fileInfo.size, fileInfo.md5);

                if (errorCode != ErrorCode::noError)
                {
                    NX_LOGX(
                        logMessage("During update storage returned error: %1").arg(
                            QnLexical::serialized(errorCode)),
                        cl_logWARNING);
                }
                NX_LOGX(logMessage("Updated file info."), cl_logINFO);

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
        NX_LOGX(
            logMessage("Requesting file info from server %1.")
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

            NX_LOGX(
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
                NX_LOGX(
                    logMessage("Cannot set checksums: %1").arg(QnLexical::serialized(errorCode)),
                    cl_logWARNING);
                return;
            }

            NX_LOGX(logMessage("Updated checksums."), cl_logDEBUG1);

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
        NX_LOGX(
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

    const auto peers = selectPeersForOperation(1, peersForChunk(chunkIndex));
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

            NX_LOGX(
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
                NX_LOGX(
                    logMessage("Cannot write chunk. Storage error: %1").arg(
                        QnLexical::serialized(errorCode)),
                    cl_logWARNING);

                // TODO: Implement error handling
                success = false;
                return;
            }
        };

    NX_LOGX(logMessage("Requesting chunk %1...").arg(chunkIndex), cl_logDEBUG2);
    const auto handle = m_peerManager->downloadChunk(peerId, m_fileName, chunkIndex, handleReply);
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
    NX_LOGX(logMessage("Download finished."), cl_logINFO);
    emit finished(m_fileName);
}

void Worker::fail()
{
    setState(State::failed);
    NX_LOGX(logMessage("Download failed."), cl_logERROR);
    emit failed(m_fileName);
}

QString Worker::logMessage(const char* message) const
{
    return lit("[%1] ").arg(m_fileName) + QString::fromLatin1(message);
}

bool Worker::addAvailableChunksInfo(const QBitArray& chunks)
{
    bool newChunksAvailable = false;

    const auto chunksCount = chunks.size();
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

QList<QnUuid> Worker::selectPeersForOperation(int count, const QList<QnUuid>& referencePeers)
{
    QList<QnUuid> result;

    auto peers = referencePeers.isEmpty() ? allPeers() : referencePeers;
    if (count <= 0)
        count = m_peersPerOperation;

    if (peers.size() <= count)
    {
        result = peers;
    }
    else
    {
        // TODO: #dklychkov Implement intelligent server selection.
        for (int i = 0; i < count; ++i)
            result.append(peers.takeAt(rand() % peers.size()));
    }

    if (result.isEmpty())
        NX_LOGX(logMessage("No peers selected."), cl_logDEBUG2);

    return result;
}

int Worker::selectNextChunk() const
{
    const auto& fileInfo = fileInformation();
    if (!fileInfo.isValid())
        return -1;

    const int chunksCount = fileInfo.downloadedChunks.size();
    const int randomChunk = rand() % chunksCount;

    for (int i = randomChunk; i < chunksCount; ++i)
    {
        if (m_availableChunks[i] && !fileInfo.downloadedChunks[i])
            return i;
    }

    for (int i = 0; i < randomChunk; ++i)
    {
        if (m_availableChunks[i] && !fileInfo.downloadedChunks[i])
            return i;
    }

    return -1;
}

void Worker::waitForNextStep(int delay)
{
    if (delay < 0)
        delay = kDefaultStepDelayMs;

    NX_LOGX(logMessage("Start waiting for next step."), cl_logDEBUG2);

    if (delay == 0)
        nextStep();
    else
        m_stepDelayTimer->start(delay);
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
