#include "update_uploader.h"

#include <QtCore/QFile>
#include <QtCore/QTimer>

#include <common/common_module.h>

#include <api/app_server_connection.h>
#include "api/model/upload_update_reply.h"
#include <nx_ec/dummy_handler.h>
#include <nx_ec/ec_api.h>
#include "core/resource_management/resource_pool.h"
#include "core/resource/media_server_resource.h"
#include "utils/update/update_utils.h"
#include <nx/utils/log/log.h>
#include <core/resource/fake_media_server.h>

namespace {
    const int chunkSize = 1024 * 1024;
    const int chunkTimeout = 5 * 60 * 1000;
    /* When a server receives the last chunk it extracts the uploaded zip file.
       This process can take long time on slow devices like ISD.
       So we have to specify a longer timeout for the last chunk. */
    const int lastChunkTimeout = 20 * 60 * 1000;

    QString getPeersString(const QSet<QnUuid> &peers) {
        QString result;
        for (const QnUuid &id: peers) {
            if (!result.isEmpty())
                result += lit(", ");
            result += id.toString();
        }
        return result;
    }
} // anonymous namespace

QnUpdateUploader::QnUpdateUploader(QObject *parent) :
    QObject(parent),
    m_chunkSize(chunkSize),
    m_chunkTimer(new QTimer(this))
{
    m_chunkTimer->setSingleShot(true);
    connect(m_chunkTimer, &QTimer::timeout, this, &QnUpdateUploader::at_chunkTimer_timeout);
}

QnUpdateUploader::~QnUpdateUploader()
{
}

QString QnUpdateUploader::updateId() const
{
    return m_updateId;
}

QSet<QnUuid> QnUpdateUploader::peers() const {
    return m_peers + m_restPeers;
}

void QnUpdateUploader::cancel() {
    cleanUp();
}

void QnUpdateUploader::sendPreambule() {
    QString md5 = makeMd5(m_updateFile.data());
    m_updateFile->seek(0);

    m_pendingPeers = m_peers + m_restPeers;

    if (!m_peers.isEmpty()) {
        NX_LOG(lit("Update: QnUpdateUploader: Send preambule transaction [%1].").arg(getPeersString(m_peers)), cl_logDEBUG2);
        commonModule()->ec2Connection()->getUpdatesManager(Qn::kSystemAccess)->sendUpdatePackageChunk(m_updateId, md5.toLatin1(), -1, m_peers, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
    }

    for (const QnMediaServerResourcePtr &server: m_restTargets) {
        NX_LOG(lit("Update: QnUpdateUploader: Send preambule request [%1 : %2].").arg(server->getName()).arg(server->getApiUrl().toString()), cl_logDEBUG2);
        int handle = server->apiConnection()->uploadUpdateChunk(m_updateId, md5.toLatin1(), -1, this, SLOT(at_restReply_finished(int,QnUploadUpdateReply,int)));
        m_restRequsts[handle] = server;
    }
}

bool QnUpdateUploader::uploadUpdate(const QString &updateId, const QString &fileName, const QSet<QnUuid> &peers) {
    if (m_updateFile)
        return false;

    NX_LOG(lit("Update: QnUpdateUploader: Starting upload [%1, %2].").arg(updateId).arg(fileName), cl_logDEBUG1);

    m_updateFile.reset(new QFile(fileName));
    if (!m_updateFile->open(QFile::ReadOnly)) {
        m_updateFile.reset();
        NX_LOG(lit("Update: QnUpdateUploader: Could not open file %1.").arg(fileName), cl_logERROR);
        return false;
    }

    m_updateId = updateId;
    m_chunkCount = (m_updateFile->size() + m_chunkSize - 1) / m_chunkSize;
    m_peers.clear();
    m_restPeers.clear();

    m_progressById.clear();
    foreach (const QnUuid &peerId, peers) {
        auto server = resourcePool()->getIncompatibleResourceById(peerId, true);
        if (!server)
            return false;

        if (server.dynamicCast<QnFakeMediaServerResource>())
        {
            m_restPeers.insert(peerId);
            m_restTargets.append(server);
        }
        else
        {
            m_peers.insert(peerId);
        }

        m_progressById[peerId] = 0;
    }

    if (!m_peers.isEmpty())
        connect(commonModule()->ec2Connection()->getUpdatesNotificationManager().get(),   &ec2::AbstractUpdatesNotificationManager::updateUploadProgress,   this,   &QnUpdateUploader::at_updateManager_updateUploadProgress);

    sendPreambule();
    return true;
}

void QnUpdateUploader::sendNextChunk() {
    if (m_updateFile.isNull())
        return;

    qint64 startChunk = 0;
    auto min = std::min_element(m_progressById.begin(), m_progressById.end());
    if (min != m_progressById.end())
        startChunk = *min;

    qint64 offset = qMin(startChunk * chunkSize, m_updateFile->size());
    m_updateFile->seek(offset);
    QByteArray data = m_updateFile->read(m_chunkSize);

    m_pendingPeers.clear();
    for (auto it = m_progressById.begin(); it != m_progressById.end(); ++it) {
        if (it.value() == startChunk)
            m_pendingPeers.insert(it.key());
    }

    if (!m_peers.isEmpty()) {
        NX_LOG(lit("Update: QnUpdateUploader: Send chunk transaction [%1, %2, %3, %4].")
               .arg(m_updateId).arg(offset).arg(data.size()).arg(getPeersString(m_pendingPeers)), cl_logDEBUG2);
        commonModule()->ec2Connection()->getUpdatesManager(Qn::kSystemAccess)->sendUpdatePackageChunk(m_updateId, data, offset, m_pendingPeers, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
        m_chunkTimer->start(data.isEmpty() ? lastChunkTimeout : chunkTimeout);
    }

    for (const QnMediaServerResourcePtr &server: m_restTargets) {
        if (!m_pendingPeers.contains(server->getId()))
            continue;
        NX_LOG(lit("Update: QnUpdateUploader: Send chunk request [%1, %2, %3, %4, %5].")
               .arg(m_updateId).arg(offset).arg(data.size()).arg(server->getId().toString()).arg(server->getApiUrl().toString()), cl_logDEBUG2);
        int handle = server->apiConnection()->uploadUpdateChunk(m_updateId, data, offset, this, SLOT(at_restReply_finished(int,QnUploadUpdateReply,int)));
        m_restRequsts[handle] = server;
    }
}

void QnUpdateUploader::handleUploadProgress(const QnUuid &peerId, qint64 chunks) {
    if (m_updateFile.isNull())
        return;

    auto it = m_progressById.find(peerId);
    if (it == m_progressById.end()) // it means we have already done upload for this peer
        return;

    NX_LOG(lit("Update: QnUpdateUploader: Got chunk response [%1, %2].").arg(peerId.toString()).arg(chunks), cl_logDEBUG2);

    int progress = 0;

    if (chunks < 0) {
        m_progressById.erase(it);

        if (chunks != ec2::AbstractUpdatesManager::NoError) {
            cleanUp();
            emit finished(chunks == ec2::AbstractUpdatesManager::NoFreeSpace ? NoFreeSpace : UnknownError, QSet<QnUuid>() << peerId);
            return;
        } else {
            progress = 100;
        }
    } else {
        if (chunks == m_updateFile->size())
            *it = m_chunkCount;
        else
            *it = chunks / chunkSize;
        progress = *it * 100 / m_chunkCount;
    }

    emit peerProgressChanged(peerId, progress);

    qint64 wholeProgress = ((m_peers + m_restPeers).size() - m_progressById.size()) * 100;
    foreach (int progress, m_progressById)
        wholeProgress += progress * 100 / m_chunkCount;
    emit progressChanged(wholeProgress / (m_peers + m_restPeers).size());

    if (m_progressById.isEmpty()) {
        cleanUp();
        NX_LOG(lit("Update: QnUpdateUploader: Upload finished."), cl_logDEBUG1);
        emit finished(NoError, QSet<QnUuid>());
    }

    m_pendingPeers.remove(peerId);
    if (m_pendingPeers.isEmpty()) {
        m_chunkTimer->stop();
        sendNextChunk();
    }
}

void QnUpdateUploader::at_updateManager_updateUploadProgress(const QString &updateId, const QnUuid &peerId, int chunks) {
    if (updateId != m_updateId)
        return;

    handleUploadProgress(peerId, chunks);
}

void QnUpdateUploader::at_chunkTimer_timeout() {
    if ((m_pendingPeers - m_restPeers).isEmpty())
        return;

    cleanUp();
    emit finished(TimeoutError, m_pendingPeers);
}

void QnUpdateUploader::at_restReply_finished(int status, const QnUploadUpdateReply &reply, int handle) {
    QnMediaServerResourcePtr server = m_restRequsts.take(handle);
    if (!server)
        return;

    if (status != 0) {
        ErrorCode errorCode = UnknownError;

        switch (status) {
        case QNetworkReply::AuthenticationRequiredError:
        case QNetworkReply::ContentAccessDenied:
            errorCode = AuthenticationError;
            break;
        case QNetworkReply::TimeoutError:
            errorCode = TimeoutError;
            break;
        default:
            break;
        }

        emit finished(errorCode, QSet<QnUuid>() << server->getId());
        return;
    }

    handleUploadProgress(server->getId(), reply.offset);
}

void QnUpdateUploader::cleanUp() {
    m_updateFile.reset();
    m_updateId.clear();
    m_chunkTimer->stop();
    commonModule()->ec2Connection()->getUpdatesNotificationManager()->disconnect(this);
}
