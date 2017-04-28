#include "worker.h"

#include <chrono>

#include <QtCore/QTimer>

#include <nx/utils/log/log.h>
#include <nx/fusion/serialization/lexical.h>

#include "storage.h"
#include "abstract_peer_manager.h"

namespace {

using namespace std::chrono;

constexpr int kStartDelayMs = milliseconds(seconds(1)).count();
constexpr int kDefaultStepDelayMs = milliseconds(minutes(1)).count();

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
    m_stepDelayTimer(new QTimer(this))
{
    NX_ASSERT(storage);
    if (!storage)
        return;

    NX_ASSERT(peerManager);
    if (!peerManager)
        return;

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

    m_started = true;
    waitForNextStep(kStartDelayMs);
}

void Worker::nextStep()
{
    const auto& fileInfo = m_storage->fileInformation(m_fileName);

    if (fileInfo.size < 0 || fileInfo.md5.isEmpty())
        findFileInformation();
    else if (fileInfo.status == DownloaderFileInformation::Status::corrupted)
        findChunksInformation();
    else if (fileInfo.status == DownloaderFileInformation::Status::downloading)
        downloadNextChunk();
    else
        waitForNextStep();
}

void Worker::findFileInformation()
{
    NX_LOGX(logMessage("Trying to get file info."), cl_logDEBUG2);

    const auto peers = m_peerManager->selectPeersForOperation();
    if (peers.isEmpty())
    {
        NX_LOGX(logMessage("No servers found."), cl_logDEBUG2);
        waitForNextStep();
        return;
    }

    auto handleReply =
        [this](bool success, rest::Handle handle, const DownloaderFileInformation& fileInfo)
        {
            const auto peer = m_peerByRequestHandle.take(handle);
            if (!peer.isNull())
                return;

            NX_LOGX(
                logMessage("Got file info reply from %1")
                    .arg(m_peerManager->peerString(peer)),
                cl_logDEBUG2);

            bool found = false;

            if (success)
            {
                if (fileInfo.isValid() && fileInfo.size >= 0 && !fileInfo.md5.isEmpty())
                {
                    const auto errorCode = m_storage->updateFileInformation(
                        fileInfo.name, fileInfo.size, fileInfo.md5);

                    cancelRequests();

                    if (errorCode != DistributedFileDownloader::ErrorCode::noError)
                    {
                        NX_LOGX(
                            logMessage("During update storage returned error: %1").arg(
                                QnLexical::serialized(errorCode)),
                            cl_logWARNING);
                    }
                    NX_LOGX(logMessage("Updated file info."), cl_logINFO);
                }
            }

            if (m_peerByRequestHandle.isEmpty())
            {
                if (found)
                    nextStep();
                else
                    waitForNextStep();
            }
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

void Worker::findChunksInformation()
{

}

void Worker::downloadNextChunk()
{

}

void Worker::cancelRequests()
{
    auto peerByRequestHandle = m_peerByRequestHandle;
    m_peerByRequestHandle.clear();

    for (auto it = peerByRequestHandle.begin(); it != peerByRequestHandle.end(); ++it)
        m_peerManager->cancelRequest(it.value(), it.key());
}

QString Worker::logMessage(const char* message) const
{
    return lit("[%1] ").arg(m_fileName) + QString::fromLatin1(message);
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

} // namespace distributed_file_downloader
} // namespace common
} // namespace vms
} // namespace nx
