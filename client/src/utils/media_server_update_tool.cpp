#include "media_server_update_tool.h"

#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtCore/QUrlQuery>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>

#include <quazip/quazip.h>
#include <quazip/quazipfile.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <common/common_module.h>
#include <utils/update/update_utils.h>
#include <utils/download_updates_peer_task.h>
#include <utils/upload_updates_peer_task.h>
#include <utils/install_updates_peer_task.h>
#include <utils/rest_update_peer_task.h>
#include <utils/applauncher_utils.h>

#include <version.h>

namespace {

const QString QN_UPDATES_URL = lit("http://enk.me/bg/dklychkov/exmaple_update/updates");
const QString QN_UPDATE_PACKAGE_URL = lit("http://enk.me/bg/dklychkov/exmaple_update/get_update");
const QString buildInformationSuffix(lit("update"));
const QString updatesDirName = lit(QN_PRODUCT_NAME_SHORT) + lit("_updates");
const QString mutexName = lit("auto_update");

bool verifyFile(const QString &fileName, qint64 size, const QString &md5) {
    QFile file(fileName);

    if (!file.exists() || file.size() != size)
        return false;

    if (!md5.isEmpty() && makeMd5(&file) != md5)
        return false;

    return true;
}

} // anonymous namespace

QnMediaServerUpdateTool::PeerUpdateInformation::PeerUpdateInformation(const QnMediaServerResourcePtr &server) :
    server(server),
    state(UpdateUnknown),
    updateInformation(0),
    progress(0)
{
    if (server)
        sourceVersion = server->getVersion();
}

// TODO: #dklychkov Split this class to a set of QnNetworkPeerTask

QnMediaServerUpdateTool::QnMediaServerUpdateTool(QObject *parent) :
    QObject(parent),
    m_state(Idle),
    m_checkResult(UpdateFound),
    m_onlineUpdateUrl(QN_UPDATES_URL),
    m_denyMajorUpdates(false),
    m_networkAccessManager(new QNetworkAccessManager(this)),
    m_distributedMutex(0),
    m_downloadUpdatesPeerTask(new QnDownloadUpdatesPeerTask(this)),
    m_uploadUpdatesPeerTask(new QnUploadUpdatesPeerTask(this)),
    m_installUpdatesPeerTask(new QnInstallUpdatesPeerTask(this)),
    m_restUpdatePeerTask(new QnRestUpdatePeerTask(this))
{
    connect(m_downloadUpdatesPeerTask,                  &QnNetworkPeerTask::finished,                   this,   &QnMediaServerUpdateTool::at_downloadTask_finished);
    connect(m_uploadUpdatesPeerTask,                    &QnNetworkPeerTask::finished,                   this,   &QnMediaServerUpdateTool::at_uploadTask_finished);
    connect(m_installUpdatesPeerTask,                   &QnNetworkPeerTask::finished,                   this,   &QnMediaServerUpdateTool::at_installTask_finished);
    connect(m_restUpdatePeerTask,                       &QnNetworkPeerTask::finished,                   this,   &QnMediaServerUpdateTool::at_restUpdateTask_finished);
    connect(m_downloadUpdatesPeerTask,                  &QnNetworkPeerTask::peerFinished,               this,   &QnMediaServerUpdateTool::at_downloadTask_peerFinished);
    connect(m_uploadUpdatesPeerTask,                    &QnNetworkPeerTask::peerFinished,               this,   &QnMediaServerUpdateTool::at_uploadTask_peerFinished);
    connect(m_installUpdatesPeerTask,                   &QnNetworkPeerTask::peerFinished,               this,   &QnMediaServerUpdateTool::at_installTask_peerFinished);
    connect(m_restUpdatePeerTask,                       &QnNetworkPeerTask::peerFinished,               this,   &QnMediaServerUpdateTool::at_restUpdateTask_peerFinished);
    connect(m_downloadUpdatesPeerTask,                  &QnNetworkPeerTask::progressChanged,            this,   &QnMediaServerUpdateTool::progressChanged);
    connect(m_uploadUpdatesPeerTask,                    &QnNetworkPeerTask::progressChanged,            this,   &QnMediaServerUpdateTool::progressChanged);
    connect(m_downloadUpdatesPeerTask,                  &QnNetworkPeerTask::peerProgressChanged,        this,   &QnMediaServerUpdateTool::at_networkTask_peerProgressChanged);
    connect(m_uploadUpdatesPeerTask,                    &QnNetworkPeerTask::peerProgressChanged,        this,   &QnMediaServerUpdateTool::at_networkTask_peerProgressChanged);
}

QnMediaServerUpdateTool::State QnMediaServerUpdateTool::state() const {
    return m_state;
}

bool QnMediaServerUpdateTool::isUpdating() const {
    return m_state >= DownloadingUpdate;
}

void QnMediaServerUpdateTool::setState(State state) {
    if (m_state == state)
        return;

    m_state = state;
    emit stateChanged(state);
}

void QnMediaServerUpdateTool::setCheckResult(QnMediaServerUpdateTool::CheckResult result) {
    m_checkResult = result;

    switch (result) {
    case UpdateFound:
        m_resultString = tr("Update has been successfully finished.");
        break;
    case InternetProblem:
        m_resultString = tr("Check for updates failed.");
        break;
    case NoNewerVersion:
        m_resultString = tr("All component in your system are already up to date.");
        break;
    case NoSuchBuild:
        m_resultString = tr("There is no such build on the update server");
        break;
    case UpdateImpossible:
        if (m_clientUpdateFile.isNull())
            m_resultString = tr("Cannot start update.\nAn update for the client was not found.");
        else
            m_resultString = tr("Cannot start update.\nAn update for one or more servers was not found.");
        break;
    case BadUpdateFile:
        m_resultString = tr("Cannot update from this file:\n%1").arg(QFileInfo(m_localUpdateFileName).fileName());
        break;
    }

    setState(Idle);
}

void QnMediaServerUpdateTool::setUpdateResult(QnMediaServerUpdateTool::UpdateResult result) {
    m_updateResult = result;

    switch (result) {
    case UpdateSuccessful:
        m_resultString = tr("Update has been successfully finished.");
        break;
    case Cancelled:
        m_resultString = tr("Update has been cancelled.");
        break;
    case LockFailed:
        m_resultString = tr("Someone has already started an update.");
        break;
    case DownloadingFailed:
        m_resultString = tr("Could not download updates.");
        break;
    case UploadingFailed:
        m_resultString = tr("Could not upload updates to servers.");
        break;
    case ClientInstallationFailed:
        m_resultString = tr("Could not install an pdate to the client.");
        break;
    case InstallationFailed:
    case RestInstallationFailed:
        m_resultString = tr("Could not install updates on one or more servers.");
        break;
    }

    setState(Idle);
}

void QnMediaServerUpdateTool::finishUpdate(QnMediaServerUpdateTool::UpdateResult result) {
    unlockMutex();
    removeTemporaryDir();
    setUpdateResult(result);
}

void QnMediaServerUpdateTool::setPeerState(const QnId &peerId, QnMediaServerUpdateTool::PeerUpdateInformation::State state) {
    auto it = m_updateInformationById.find(peerId);
    if (it == m_updateInformationById.end())
        return;

    if (it->state != state) {
        it->state = state;
        emit peerChanged(peerId);
    }
}

QnMediaServerUpdateTool::CheckResult QnMediaServerUpdateTool::updateCheckResult() const {
    return m_checkResult;
}

QnMediaServerUpdateTool::UpdateResult QnMediaServerUpdateTool::updateResult() const {
    return m_updateResult;
}

QString QnMediaServerUpdateTool::resultString() const {
    return m_resultString;
}

QnSoftwareVersion QnMediaServerUpdateTool::targetVersion() const {
    return m_targetVersion;
}

void QnMediaServerUpdateTool::setDenyMajorUpdates(bool denyMajorUpdates) {
    m_denyMajorUpdates = denyMajorUpdates;
}

QnMediaServerUpdateTool::PeerUpdateInformation QnMediaServerUpdateTool::updateInformation(const QnId &peerId) const {
    auto it = m_updateInformationById.find(peerId);
    if (it != m_updateInformationById.end())
        return it.value();

    PeerUpdateInformation info(qnResPool->getIncompatibleResourceById(peerId, true).dynamicCast<QnMediaServerResource>());
    if (info.server && m_state == Idle) {
        info.updateInformation = m_updateFiles[info.server->getSystemInfo()];
        if (m_targetVersion.isNull())
            info.state = PeerUpdateInformation::UpdateUnknown;
        else
            info.state = info.updateInformation ? PeerUpdateInformation::UpdateFound : PeerUpdateInformation::UpdateNotFound;
    }
    return info;
}

QnMediaServerResourceList QnMediaServerUpdateTool::targets() const {
    return m_targets;
}

void QnMediaServerUpdateTool::setTargets(const QSet<QnId> &targets) {
    m_targets.clear();

    foreach (const QnId &id, targets) {
        QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(id).dynamicCast<QnMediaServerResource>();
        if (!server)
            continue;
        m_targets.append(server);
    }
}

QnMediaServerResourceList QnMediaServerUpdateTool::actualTargets() const {
    QnMediaServerResourceList result;

    if (m_targets.isEmpty()) {
        foreach (const QnResourcePtr &resource, qnResPool->getResourcesWithFlag(QnResource::server))
            result.append(resource.staticCast<QnMediaServerResource>());

        foreach (const QnResourcePtr &resource, qnResPool->getAllIncompatibleResources()) {
            if (QnMediaServerResourcePtr server = resource.dynamicCast<QnMediaServerResource>()) {
                if (server->getSystemName() == qnCommon->localSystemName())
                    result.append(server);
            }
        }

    } else {
        result = m_targets;
    }

    return result;
}

QUrl QnMediaServerUpdateTool::generateUpdatePackageUrl() const {
    QUrlQuery query;

    QString versionSuffix;
    if (m_targetVersion.isNull()) {
        versionSuffix = lit("/latest");
        query.addQueryItem(lit("current"), qnCommon->engineVersion().toString());
    } else {
        versionSuffix = QString(lit("/%1-%2")).arg(m_targetVersion.toString(), passwordForBuild((unsigned)m_targetVersion.build()));
    }

    QSet<QnSystemInformation> systemInformationList;

    if (m_idBySystemInformation.isEmpty()) {
        foreach (const QnMediaServerResourcePtr &server, actualTargets())
            systemInformationList.insert(server->getSystemInfo());
    } else {
        systemInformationList = QSet<QnSystemInformation>::fromList(m_idBySystemInformation.keys());
    }

    query.addQueryItem(lit("client"), QnSystemInformation(lit(QN_APPLICATION_PLATFORM), lit(QN_APPLICATION_ARCH), lit(QN_ARM_BOX)).toString().replace(QLatin1Char(' '), QLatin1Char('_')));
    foreach (const QnSystemInformation &systemInformation, systemInformationList)
        query.addQueryItem(lit("server"), systemInformation.toString().replace(QLatin1Char(' '), QLatin1Char('_')));

    QUrl url(QN_UPDATE_PACKAGE_URL + versionSuffix);
    url.setQuery(query);

    return url;
}

void QnMediaServerUpdateTool::reset() {
    if (m_state != Idle)
        return;

    m_updateInformationById.clear();
    m_updateFiles.clear();
    m_clientUpdateFile.clear();
    m_targetVersion = QnSoftwareVersion();
}

void QnMediaServerUpdateTool::checkForUpdates() {
    if (m_state >= CheckingForUpdates)
        return;

    checkOnlineUpdates();
}

void QnMediaServerUpdateTool::checkForUpdates(const QnSoftwareVersion &version) {
    if (m_state >= CheckingForUpdates)
        return;

    checkOnlineUpdates(version);
}

void QnMediaServerUpdateTool::checkForUpdates(const QString &fileName) {
    if (m_state >= CheckingForUpdates)
        return;

    m_localUpdateFileName = fileName;

    checkLocalUpdates();
}

void QnMediaServerUpdateTool::checkOnlineUpdates(const QnSoftwareVersion &version) {
    setState(CheckingForUpdates);

    if (m_denyMajorUpdates && !version.isNull()) {
        QnSoftwareVersion currentVersion = qnCommon->engineVersion();
        if (version.major() != currentVersion.major() || version.minor() != currentVersion.minor()) {
            setCheckResult(NoSuchBuild);
            return;
        }
    }

    m_updateFiles.clear();
    m_clientUpdateFile.clear();
    m_updateInformationById.clear();

    m_targetMustBeNewer = version.isNull();
    m_targetVersion = version;

    QNetworkReply *reply = m_networkAccessManager->get(QNetworkRequest(QUrl(m_onlineUpdateUrl)));
    connect(reply, &QNetworkReply::finished, this, &QnMediaServerUpdateTool::at_updateReply_finished);
}

void QnMediaServerUpdateTool::checkLocalUpdates() {
    m_updateFiles.clear();
    m_clientUpdateFile.clear();
    m_updateInformationById.clear();
    m_targetMustBeNewer = false;
    m_targetVersion = QnSoftwareVersion();
    m_localTemporaryDir.clear();

    setState(CheckingForUpdates);

    if (!QFile::exists(m_localUpdateFileName)) {
        setCheckResult(BadUpdateFile);
        return;
    }

    QDir dir = QDir::temp();
    forever {
        QString dirName = QUuid::createUuid().toString();
        if (dir.exists(dirName))
            continue;

        if (!dir.mkdir(dirName)) {
            setCheckResult(BadUpdateFile);
            return;
        }

        dir.cd(dirName);
        break;
    }

    if (!extractZipArchive(m_localUpdateFileName, dir)) {
        removeTemporaryDir();
        setCheckResult(BadUpdateFile);
        return;
    }

    QRegExp updateFileRegExp(lit("(?:client_){0,1}update_.+_.+_\\d+\\.\\d+\\.\\d+\\.\\d+\\.zip"));

    QStringList entries = dir.entryList(QStringList() << lit("*.zip"), QDir::Files);
    foreach (const QString &entry, entries) {
        if (!updateFileRegExp.exactMatch(entry))
            continue;

        QString fileName = dir.absoluteFilePath(entry);
        QnSoftwareVersion version;
        QnSystemInformation sysInfo;
        bool isClient;

        if (!verifyUpdatePackage(fileName, &version, &sysInfo, &isClient))
            continue;

        if (m_updateFiles.contains(sysInfo))
            continue;

        if (m_targetVersion.isNull())
            m_targetVersion = version;

        if (m_targetVersion != version) {
            setCheckResult(BadUpdateFile);
            return;
        }

        UpdateFileInformationPtr updateFileInformation(new UpdateFileInformation(version, fileName));
        QFile file(fileName);
        updateFileInformation->fileSize = file.size();
        updateFileInformation->md5 = makeMd5(&file);
        if (isClient)
            m_clientUpdateFile = updateFileInformation;
        else
            m_updateFiles.insert(sysInfo, updateFileInformation);
    }

    checkUpdateCoverage();
}

void QnMediaServerUpdateTool::checkBuildOnline() {
    setState(CheckingForUpdates);
    m_targetMustBeNewer = false;

    QUrl url(m_updateLocationPrefix + QString::number(m_targetVersion.build()) + lit("/") + buildInformationSuffix);
    QNetworkReply *reply = m_networkAccessManager->get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, &QnMediaServerUpdateTool::at_buildReply_finished);
}

void QnMediaServerUpdateTool::removeTemporaryDir() {
    if (m_localTemporaryDir.isEmpty())
        return;

    QDir(m_localTemporaryDir).removeRecursively();
    m_localTemporaryDir.clear();
}

void QnMediaServerUpdateTool::checkUpdateCoverage() {
    bool needUpdate = false;
    foreach (const QnMediaServerResourcePtr &server, actualTargets()) {
        UpdateFileInformationPtr updateFileInformation = m_updateFiles[server->getSystemInfo()];
        if (!updateFileInformation) {
            removeTemporaryDir();
            setCheckResult(UpdateImpossible);
            return;
        }
        QnSoftwareVersion version = updateFileInformation->version;
        if ((m_targetMustBeNewer && version > server->getVersion()) || (!m_targetMustBeNewer && version != server->getVersion()))
            needUpdate = true;
    }

    if (!m_clientUpdateFile) {
        removeTemporaryDir();
        setCheckResult(UpdateImpossible);
        return;
    }

    setCheckResult(needUpdate ? UpdateFound : NoNewerVersion);
    return;
}

void QnMediaServerUpdateTool::at_updateReply_finished() {
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        Q_ASSERT_X(0, "This function must be called only from QNetworkReply", Q_FUNC_INFO);
        return;
    }

    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        setCheckResult(InternetProblem);
        return;
    }

    QByteArray data = reply->readAll();
    QVariantMap map = QJsonDocument::fromJson(data).toVariant().toMap();
    map = map.value(lit(QN_CUSTOMIZATION_NAME)).toMap();
    QnSoftwareVersion latestVersion = QnSoftwareVersion(map.value(lit("latest_version")).toString());
    QString updatesPrefix = map.value(lit("updates_prefix")).toString();
    if (latestVersion.isNull() || updatesPrefix.isEmpty()) {
        setCheckResult(InternetProblem);
        return;
    }

    if (m_targetVersion.isNull())
        m_targetVersion = latestVersion;
    m_updateLocationPrefix = updatesPrefix;

    checkBuildOnline();
}

void QnMediaServerUpdateTool::at_buildReply_finished() {
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        Q_ASSERT_X(0, "This function must be called only from QNetworkReply", Q_FUNC_INFO);
        return;
    }

    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        setCheckResult((reply->error() == QNetworkReply::ContentNotFoundError) ? NoSuchBuild : InternetProblem);
        return;
    }

    QByteArray data = reply->readAll();
    QVariantMap map = QJsonDocument::fromJson(data).toVariant().toMap();

    m_targetVersion = QnSoftwareVersion(map.value(lit("version")).toString());

    if (m_targetVersion.isNull()) {
        setCheckResult(NoSuchBuild);
        return;
    }

    QString urlPrefix = m_updateLocationPrefix + QString::number(m_targetVersion.build()) + lit("/");

    QVariantMap packages = map.value(lit("packages")).toMap();
    for (auto platform = packages.begin(); platform != packages.end(); ++platform) {
        QVariantMap architectures = platform.value().toMap();
        for (auto arch = architectures.begin(); arch != architectures.end(); ++arch) {
            // We suppose arch name does not contain '_' char. E.g. arm_isd_s2 will be split to "arm" and "isd_s2"
            QString architecture = arch.key();
            QString modification;
            int i = architecture.indexOf(QChar::fromLatin1('_'));
            if (i != -1) {
                modification = architecture.mid(i + 1);
                architecture = architecture.left(i);
            }

            QVariantMap package = arch.value().toMap();
            QString fileName = package.value(lit("file")).toString();
            UpdateFileInformationPtr info(new UpdateFileInformation(m_targetVersion, QUrl(urlPrefix + fileName)));
            info->baseFileName = fileName;
            info->fileSize = package.value(lit("size")).toLongLong();
            info->md5 = package.value(lit("md5")).toString();
            m_updateFiles.insert(QnSystemInformation(platform.key(), architecture, modification), info);
        }
    }

    packages = map.value(lit("client_packages")).toMap();
    QString arch = lit(QN_APPLICATION_ARCH);
    QString modification = lit(QN_ARM_BOX);
    if (!modification.isEmpty())
        arch += lit("_") + modification;
    QVariantMap package = packages.value(lit(QN_APPLICATION_PLATFORM)).toMap().value(arch).toMap();

    if (!package.isEmpty()) {
        QString fileName = package.value(lit("file")).toString();
        m_clientUpdateFile.reset(new UpdateFileInformation(m_targetVersion, QUrl(urlPrefix + fileName)));
        m_clientUpdateFile->baseFileName = fileName;
        m_clientUpdateFile->fileSize = package.value(lit("size")).toLongLong();
        m_clientUpdateFile->md5 = package.value(lit("md5")).toString();
    }

    checkUpdateCoverage();
}


void QnMediaServerUpdateTool::updateServers() {
    m_updateId = QUuid::createUuid().toString();
    m_updateInformationById.clear();
    m_idBySystemInformation.clear();

    foreach (const QnMediaServerResourcePtr &server, actualTargets()) {
        bool incompatible = (server->getStatus() == QnResource::Incompatible);

        if (server->getStatus() != QnResource::Online && !incompatible)
            continue;

        if (!server->getSystemInfo().isValid())
            continue;

        QnId peerId = server->getId();

        m_targetPeerIds.insert(peerId);
        if (incompatible)
            m_incompatiblePeerIds.insert(peerId);

        PeerUpdateInformation info(server);
        info.updateInformation = m_updateFiles.value(server->getSystemInfo());

        if (!info.updateInformation) {
            Q_ASSERT_X(0, "No update info for server", Q_FUNC_INFO);
            return;
        }

        info.state = info.updateInformation->fileName.isEmpty() ? PeerUpdateInformation::PendingDownloading
                                                                : PeerUpdateInformation::PendingUpload;

        m_updateInformationById.insert(peerId, info);
        m_idBySystemInformation.insert(server->getSystemInfo(), peerId);
    }

    setState(DownloadingUpdate);

    downloadUpdates();
}

bool QnMediaServerUpdateTool::cancelUpdate() {
    switch (m_state) {
    case DownloadingUpdate:
        m_downloadUpdatesPeerTask->cancel();
        break;
    case InstallingToIncompatiblePeers:
        m_restUpdatePeerTask->cancel();
        break;
    case UploadingUpdate:
        m_uploadUpdatesPeerTask->cancel();
        break;
    case InstallingUpdate:
        return false;
    default:
        break;
    }

    unlockMutex();

    for (auto it = m_updateInformationById.begin(); it != m_updateInformationById.end(); ++it)
        it->state = PeerUpdateInformation::UpdateCanceled;

    setState(Idle);
    return true;
}

void QnMediaServerUpdateTool::downloadUpdates() {
    QHash<QUrl, QString> downloadTargets;
    QMultiHash<QUrl, QnId> peerAssociations;
    QHash<QUrl, QString> hashByUrl;
    QHash<QUrl, qint64> fileSizeByUrl;

    for (auto it = m_updateFiles.begin(); it != m_updateFiles.end(); ++it) {
        QList<QnId> peers = m_idBySystemInformation.values(it.key());
        if (peers.isEmpty())
            continue;

        QString fileName = it.value()->fileName;
        if (fileName.isEmpty())
            fileName = updateFilePath(updatesDirName, it.value()->baseFileName);

        if (!fileName.isEmpty() && verifyFile(fileName, it.value()->fileSize, it.value()->md5)) {
            it.value()->fileName = fileName;
            continue;
        }

        downloadTargets.insert(it.value()->url, it.value()->baseFileName);
        hashByUrl.insert(it.value()->url, it.value()->md5);
        fileSizeByUrl.insert(it.value()->url, it.value()->fileSize);
        foreach (const QnId &peerId, peers) {
            peerAssociations.insert(it.value()->url, peerId);
            PeerUpdateInformation &updateInformation = m_updateInformationById[peerId];
            updateInformation.state = PeerUpdateInformation::UpdateDownloading;
            updateInformation.progress = 0;
        }
    }

    QString fileName = m_clientUpdateFile->fileName;
    if (fileName.isEmpty())
        fileName = updateFilePath(updatesDirName, m_clientUpdateFile->baseFileName);

    if (!fileName.isEmpty() && verifyFile(fileName, m_clientUpdateFile->fileSize, m_clientUpdateFile->md5)) {
        m_clientUpdateFile->fileName = fileName;
    } else {
        downloadTargets.insert(m_clientUpdateFile->url, m_clientUpdateFile->baseFileName);
        hashByUrl.insert(m_clientUpdateFile->url, m_clientUpdateFile->md5);
        fileSizeByUrl.insert(m_clientUpdateFile->url, m_clientUpdateFile->fileSize);
        peerAssociations.insert(m_clientUpdateFile->url, qnCommon->moduleGUID());
    }

    emit progressChanged(0);
    setState(DownloadingUpdate);

    m_downloadUpdatesPeerTask->setTargetDir(updatesDirName);
    m_downloadUpdatesPeerTask->setTargets(downloadTargets);
    m_downloadUpdatesPeerTask->setHashes(hashByUrl);
    m_downloadUpdatesPeerTask->setFileSizes(fileSizeByUrl);
    m_downloadUpdatesPeerTask->setPeerAssociations(peerAssociations);
    m_downloadUpdatesPeerTask->start(QSet<QnId>::fromList(m_updateInformationById.keys()));
}

void QnMediaServerUpdateTool::uploadUpdatesToServers() {
    QHash<QnSystemInformation, QString> fileBySystemInformation;
    for (auto it = m_updateFiles.begin(); it != m_updateFiles.end(); ++it)
        fileBySystemInformation[it.key()] = it.value()->fileName;

    for (auto it = m_updateInformationById.begin(); it != m_updateInformationById.end(); ++it) {
        if (!m_incompatiblePeerIds.contains(it.key()))
            it->state = PeerUpdateInformation::UpdateUploading;
    }

    emit progressChanged(0);
    setState(UploadingUpdate);

    m_uploadUpdatesPeerTask->setUpdateId(m_updateId);
    m_uploadUpdatesPeerTask->setUploads(fileBySystemInformation);
    m_uploadUpdatesPeerTask->start(m_targetPeerIds - m_incompatiblePeerIds);
}

void QnMediaServerUpdateTool::installClientUpdate() {
    setState(InstallingClientUpdate);

    if (applauncher::installZip(m_targetVersion, m_clientUpdateFile->fileName) != applauncher::api::ResultType::ok) {
        for (auto it = m_updateInformationById.begin(); it != m_updateInformationById.end(); ++it)
            setPeerState(it.key(), PeerUpdateInformation::UpdateFailed);
        finishUpdate(InstallationFailed);
        return;
    }

    installIncompatiblePeers();
}

void QnMediaServerUpdateTool::installUpdatesToServers() {
    for (auto it = m_updateInformationById.begin(); it != m_updateInformationById.end(); ++it) {
        if (!m_incompatiblePeerIds.contains(it.key()))
            it->state = PeerUpdateInformation::UpdateInstalling;
    }

    setState(InstallingUpdate);

    m_installUpdatesPeerTask->setUpdateId(m_updateId);
    m_installUpdatesPeerTask->setVersion(m_targetVersion);
    m_installUpdatesPeerTask->start(m_targetPeerIds - m_incompatiblePeerIds);
}

void QnMediaServerUpdateTool::installIncompatiblePeers() {
    QHash<QnSystemInformation, QString> updateFiles;
    for (auto it = m_updateFiles.begin(); it != m_updateFiles.end(); ++it) {
        updateFiles[it.key()] = it.value()->fileName;

        foreach (const QnId &peerId, m_idBySystemInformation.values(it.key())) {
            if (m_incompatiblePeerIds.contains(peerId))
                m_updateInformationById[peerId].state = PeerUpdateInformation::UpdateInstalling;
        }
    }

    setState(InstallingToIncompatiblePeers);

    m_restUpdatePeerTask->setUpdateId(m_updateId);
    m_restUpdatePeerTask->setUpdateFiles(updateFiles);
    m_restUpdatePeerTask->setVersion(m_targetVersion);
    m_restUpdatePeerTask->start(m_incompatiblePeerIds);
}

void QnMediaServerUpdateTool::lockMutex() {
    m_distributedMutex = ec2::QnDistributedMutexManager::instance()->createMutex(mutexName);
    connect(m_distributedMutex, &ec2::QnDistributedMutex::locked,        this,   &QnMediaServerUpdateTool::at_mutexLocked, Qt::QueuedConnection);
    connect(m_distributedMutex, &ec2::QnDistributedMutex::lockTimeout,   this,   &QnMediaServerUpdateTool::at_mutexTimeout, Qt::QueuedConnection);
    m_distributedMutex->lockAsync();
}

void QnMediaServerUpdateTool::unlockMutex() {
    if (m_distributedMutex) {
        m_distributedMutex->unlock();
        m_distributedMutex->deleteLater();
        m_distributedMutex = 0;
    }
}

void QnMediaServerUpdateTool::at_mutexLocked() {
    uploadUpdatesToServers();
}

void QnMediaServerUpdateTool::at_mutexTimeout() {
    m_distributedMutex->deleteLater();
    m_distributedMutex = 0;
    finishUpdate(LockFailed);
}

void QnMediaServerUpdateTool::at_downloadTask_finished(int errorCode) {
    if (m_state != DownloadingUpdate)
        return;

    if (errorCode != 0) {
        for (auto it = m_updateInformationById.begin(); it != m_updateInformationById.end(); ++it)
            setPeerState(it.key(), PeerUpdateInformation::UpdateFailed);
        finishUpdate(DownloadingFailed);
        return;
    }

    QHash<QUrl, QString> resultingFiles = m_downloadUpdatesPeerTask->resultingFiles();

    for (auto it = m_updateFiles.begin(); it != m_updateFiles.end(); ++it) {
        if (resultingFiles.contains(it.value()->url))
            it.value()->fileName = resultingFiles[it.value()->url];
    }
    if (resultingFiles.contains(m_clientUpdateFile->url))
        m_clientUpdateFile->fileName = resultingFiles[m_clientUpdateFile->url];

    installClientUpdate();
}

void QnMediaServerUpdateTool::at_uploadTask_finished(int errorCode) {
    if (m_state != UploadingUpdate)
        return;

    if (errorCode != 0) {
        for (auto it = m_updateInformationById.begin(); it != m_updateInformationById.end(); ++it)
            setPeerState(it.key(), PeerUpdateInformation::UpdateFailed);
        finishUpdate(UploadingFailed);
        return;
    }

    installUpdatesToServers();
}

void QnMediaServerUpdateTool::at_installTask_finished(int errorCode) {
    if (m_state != InstallingUpdate)
        return;

    if (errorCode != 0) {
        for (auto it = m_updateInformationById.begin(); it != m_updateInformationById.end(); ++it)
            setPeerState(it.key(), PeerUpdateInformation::UpdateFailed);
        finishUpdate(InstallationFailed);
        return;
    }

    finishUpdate(UpdateSuccessful);
}

void QnMediaServerUpdateTool::at_restUpdateTask_finished(int errorCode) {
    if (m_state != InstallingToIncompatiblePeers)
        return;

    if (errorCode != 0) {
        for (auto it = m_updateInformationById.begin(); it != m_updateInformationById.end(); ++it)
            setPeerState(it.key(), PeerUpdateInformation::UpdateFailed);
        finishUpdate(RestInstallationFailed);
        return;
    }

    if ((m_targetPeerIds - m_incompatiblePeerIds).isEmpty()) {
        finishUpdate(UpdateSuccessful);
        return;
    }

    lockMutex();
}

void QnMediaServerUpdateTool::at_networkTask_peerProgressChanged(const QnId &peerId, int progress) {
    m_updateInformationById[peerId].progress = progress;
    emit peerChanged(peerId);
}

void QnMediaServerUpdateTool::at_downloadTask_peerFinished(const QnId &peerId) {
    setPeerState(peerId, PeerUpdateInformation::PendingUpload);
}

void QnMediaServerUpdateTool::at_uploadTask_peerFinished(const QnId &peerId) {
    setPeerState(peerId, PeerUpdateInformation::PendingInstallation);
}

void QnMediaServerUpdateTool::at_installTask_peerFinished(const QnId &peerId) {
    setPeerState(peerId, PeerUpdateInformation::UpdateFinished);
}

void QnMediaServerUpdateTool::at_restUpdateTask_peerFinished(const QnId &peerId) {
    setPeerState(peerId, PeerUpdateInformation::UpdateFinished);
}
