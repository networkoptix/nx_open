#include "update_uploader.h"

#include <QtCore/QFile>
#include <QtCore/QTimer>

#include <api/app_server_connection.h>
#include "api/model/upload_update_reply.h"
#include <nx_ec/dummy_handler.h>
#include <nx_ec/ec_api.h>
#include "core/resource_management/resource_pool.h"
#include "core/resource/media_server_resource.h"
#include "utils/update/update_utils.h"

namespace {
    const int chunkSize = 1024 * 1024;
    const int chunkTimeout = 5 * 60 * 1000;

    ec2::AbstractECConnectionPtr connection2() {
        return QnAppServerConnectionFactory::getConnection2();
    }
} // anonymous namespace

QnUpdateUploader::QnUpdateUploader(QObject *parent) :
    QObject(parent),
    m_chunkSize(chunkSize),
    m_chunkTimer(new QTimer(this))
{
    m_chunkTimer->setInterval(chunkTimeout);
    m_chunkTimer->setSingleShot(true);
    connect(m_chunkTimer, &QTimer::timeout, this, &QnUpdateUploader::at_chunkTimer_timeout);
}

QString QnUpdateUploader::updateId() const {
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

    if (!m_peers.isEmpty())
        connection2()->getUpdatesManager()->sendUpdatePackageChunk(m_updateId, md5.toLatin1(), -1, m_peers, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);

    for (const QnMediaServerResourcePtr &server: m_restTargets) {
        int handle = server->apiConnection()->uploadUpdateChunk(m_updateId, md5.toLatin1(), -1, this, SLOT(at_restReply_finished(int,QnUploadUpdateReply,int)));
        m_restRequsts[handle] = server;
    }
}

bool QnUpdateUploader::uploadUpdate(const QString &updateId, const QString &fileName, const QSet<QnUuid> &peers) {
    if (m_updateFile)
        return false;

    m_updateFile.reset(new QFile(fileName));
    if (!m_updateFile->open(QFile::ReadOnly)) {
        m_updateFile.reset();
        return false;
    }

    m_updateId = updateId;
    m_chunkCount = (m_updateFile->size() + m_chunkSize - 1) / m_chunkSize;
    m_peers.clear();
    m_restPeers.clear();

    m_progressById.clear();
    foreach (const QnUuid &peerId, peers) {
        QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(peerId, true).dynamicCast<QnMediaServerResource>();
        if (!server)
            return false;

        if (server->hasProperty(lit("guid"))) {
            m_restPeers.insert(peerId);
            m_restTargets.append(server);
        } else {
            m_peers.insert(peerId);
        }

        m_progressById[peerId] = 0;
    }

    if (!m_peers.isEmpty())
        connect(connection2()->getUpdatesManager().get(),   &ec2::AbstractUpdatesManager::updateUploadProgress,   this,   &QnUpdateUploader::at_updateManager_updateUploadProgress);

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
        connection2()->getUpdatesManager()->sendUpdatePackageChunk(m_updateId, data, offset, m_pendingPeers, ec2::DummyHandler::instance(), &ec2::DummyHandler::onRequestDone);
        m_chunkTimer->start();
    }

    for (const QnMediaServerResourcePtr &server: m_restTargets) {
        if (!m_pendingPeers.contains(server->getId()))
            continue;
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
        emit finished(UnknownError, QSet<QnUuid>() << server->getId());
        return;
    }

    handleUploadProgress(server->getId(), reply.offset);
}

void QnUpdateUploader::cleanUp() {
    m_updateFile.reset();
    m_updateId.clear();
    m_chunkTimer->stop();
    connection2()->getUpdatesManager()->disconnect(this);
}
