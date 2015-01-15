#include "check_update_peer_task.h"

#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

#include <quazip/quazip.h>
#include <quazip/quazipfile.h>

#include <client/client_settings.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <common/common_module.h>

#include <utils/update/update_utils.h>
#include <utils/update/zip_utils.h>
#include <utils/applauncher_utils.h>
#include <utils/network/http/async_http_client_reply.h>

#include <utils/common/app_info.h>

#define QnNetworkReplyError static_cast<void (QNetworkReply::*)(QNetworkReply::NetworkError)>(&QNetworkReply::error)

namespace {
    const QString buildInformationSuffix = lit("update.json");
    const QString updateInformationFileName = (lit("update.json"));

    const int httpResponseTimeoutMs = 30000;

    QnSoftwareVersion minimalVersionForUpdatePackage(const QString &fileName) {
        QuaZipFile infoFile(fileName, updateInformationFileName);
        if (!infoFile.open(QuaZipFile::ReadOnly))
            return QnSoftwareVersion();

        QString data = QString::fromUtf8(infoFile.readAll());
        infoFile.close();

        QRegExp minimalVersionRegExp(lit("\"minimalVersion\"\\s*:\\s*\"([\\d\\.]+)\""));
        if (minimalVersionRegExp.indexIn(data) != -1)
            return QnSoftwareVersion(minimalVersionRegExp.cap(1));

        return QnSoftwareVersion();
    }

    QnSoftwareVersion maximumAvailableVersion() {
        QList<QnSoftwareVersion> versions;
        if (applauncher::getInstalledVersions(&versions) != applauncher::api::ResultType::ok)
            versions.append(QnSoftwareVersion(qApp->applicationVersion()));

        return *std::max_element(versions.begin(), versions.end());
    }
}

QnCheckForUpdatesPeerTask::QnCheckForUpdatesPeerTask(const QnUpdateTarget &target, QObject *parent) :
    QnNetworkPeerTask(parent),
    m_updatesUrl(QUrl(qnSettings->updateFeedUrl())),
    m_target(target)
{
}

QHash<QnSystemInformation, QnUpdateFileInformationPtr> QnCheckForUpdatesPeerTask::updateFiles() const {
    return m_updateFiles;
}

QnUpdateFileInformationPtr QnCheckForUpdatesPeerTask::clientUpdateFile() const {
    return m_clientUpdateFile;
}

void QnCheckForUpdatesPeerTask::doStart() {
    if (!m_target.fileName.isEmpty())
        checkLocalUpdates();
    else
        checkOnlineUpdates();
}

bool QnCheckForUpdatesPeerTask::needUpdate(const QnSoftwareVersion &version, const QnSoftwareVersion &updateVersion) const {
    return (m_targetMustBeNewer && updateVersion > version) || (!m_targetMustBeNewer && updateVersion != version);
}

void QnCheckForUpdatesPeerTask::checkUpdateCoverage() {
    if (m_updateFiles.isEmpty()) {
        finishTask(QnCheckForUpdateResult::BadUpdateFile);
        return;
    }

    bool needUpdate = false;
    foreach (const QnUuid &peerId, peers()) {
        QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(peerId, true).dynamicCast<QnMediaServerResource>();
        if (!server)
            continue;

        bool updateServer = this->needUpdate(server->getVersion(), m_target.version);
        if (updateServer && !m_updateFiles.value(server->getSystemInfo())) {
            finishTask(QnCheckForUpdateResult::ServerUpdateImpossible);
            return;
        }
        needUpdate |= updateServer;
    }

    if (!m_target.denyClientUpdates && !m_clientRequiresInstaller) {
        bool updateClient = this->needUpdate(qnCommon->engineVersion(), m_target.version);

        if (updateClient && !m_clientUpdateFile) {
            finishTask(QnCheckForUpdateResult::ClientUpdateImpossible);
            return;
        }

        needUpdate |= updateClient;
    }

    finishTask(needUpdate ? QnCheckForUpdateResult::UpdateFound : QnCheckForUpdateResult::NoNewerVersion);
    return;
}

void QnCheckForUpdatesPeerTask::checkBuildOnline() {
    QUrl url(m_updateLocationPrefix + lit("/") + QString::number(m_target.version.build()) + lit("/") + buildInformationSuffix);

    nx_http::AsyncHttpClientPtr httpClient = std::make_shared<nx_http::AsyncHttpClient>();
    httpClient->setResponseReadTimeoutMs(httpResponseTimeoutMs);
    QnAsyncHttpClientReply *reply = new QnAsyncHttpClientReply(httpClient);
    connect(reply, &QnAsyncHttpClientReply::finished, this, &QnCheckForUpdatesPeerTask::at_buildReply_finished);
    if (!httpClient->doGet(url))
        finishTask(QnCheckForUpdateResult::InternetProblem);
    else 
        m_runningRequests.insert(reply);
}

void QnCheckForUpdatesPeerTask::checkOnlineUpdates() {
    if (m_target.denyMajorUpdates && !m_target.version.isNull()) {
        QnSoftwareVersion currentVersion = qnCommon->engineVersion();
        if (m_target.version.major() != currentVersion.major() || m_target.version.minor() != currentVersion.minor()) {
            finishTask(QnCheckForUpdateResult::NoSuchBuild);
            return;
        }
    }

    m_updateFiles.clear();
    m_clientUpdateFile.clear();
    m_releaseNotesUrl.clear();

    m_targetMustBeNewer = m_target.version.isNull();
    m_checkLatestVersion = m_target.version.isNull();

    nx_http::AsyncHttpClientPtr httpClient = std::make_shared<nx_http::AsyncHttpClient>();
    httpClient->setResponseReadTimeoutMs(httpResponseTimeoutMs);
    QnAsyncHttpClientReply *reply = new QnAsyncHttpClientReply(httpClient);
    connect(reply, &QnAsyncHttpClientReply::finished, this, &QnCheckForUpdatesPeerTask::at_updateReply_finished);
    if (!httpClient->doGet(m_updatesUrl))
        finishTask(QnCheckForUpdateResult::InternetProblem);
    else 
        m_runningRequests.insert(reply);
}

void QnCheckForUpdatesPeerTask::checkLocalUpdates() {
    m_updateFiles.clear();
    m_clientUpdateFile.clear();
    m_targetMustBeNewer = false;
    m_releaseNotesUrl.clear();

    if (!QFile::exists(m_target.fileName)) {
        finishTask(QnCheckForUpdateResult::BadUpdateFile);
        return;
    }

    QnZipExtractor *extractor(new QnZipExtractor(m_target.fileName, updatesCacheDir()));
    connect(extractor, &QnZipExtractor::finished, this, &QnCheckForUpdatesPeerTask::at_zipExtractor_finished);
    extractor->start();
}

void QnCheckForUpdatesPeerTask::at_updateReply_finished(QnAsyncHttpClientReply *reply) {
    if (!m_runningRequests.remove(reply))
        return;

    reply->deleteLater();

    if (reply->isFailed()) {
        finishTask(QnCheckForUpdateResult::InternetProblem);
        return;
    }

    QByteArray data = reply->data();
    
    QVariantMap map = QJsonDocument::fromJson(data).toVariant().toMap();
    map = map.value(QnAppInfo::customizationName()).toMap();
    QVariantMap releasesMap = map.value(lit("releases")).toMap();

    QString currentRelease = map.value(lit("current_release")).toString();
    if (m_target.denyMajorUpdates || QnSoftwareVersion(currentRelease) < qnCommon->engineVersion())
        currentRelease = qnCommon->engineVersion().toString(QnSoftwareVersion::MinorFormat);

    QnSoftwareVersion latestVersion = QnSoftwareVersion(releasesMap.value(currentRelease).toString());
    QString updatesPrefix = map.value(lit("updates_prefix")).toString();
    if (latestVersion.isNull() || updatesPrefix.isEmpty()) {
        finishTask(QnCheckForUpdateResult::NoNewerVersion);
        return;
    }


    if (m_target.version.isNull())
        m_target.version = latestVersion;

    m_updateLocationPrefix = updatesPrefix;
    m_releaseNotesUrl = QUrl(map.value(lit("release_notes")).toString());

    checkBuildOnline();
}

void QnCheckForUpdatesPeerTask::at_buildReply_finished(QnAsyncHttpClientReply *reply) {
    if (!m_runningRequests.remove(reply))
        return;

    reply->deleteLater();

    if (reply->isFailed()) {
        finishTask(QnCheckForUpdateResult::NoSuchBuild);
        return;
    }

    QByteArray data = reply->data();
    QVariantMap map = QJsonDocument::fromJson(data).toVariant().toMap();

    m_target.version = QnSoftwareVersion(map.value(lit("version")).toString());

    if (m_target.version.isNull()) {
        finishTask(QnCheckForUpdateResult::NoSuchBuild);
        return;
    }

    QString urlPrefix = m_updateLocationPrefix + lit("/") + QString::number(m_target.version.build()) + lit("/");

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
            QnUpdateFileInformationPtr info(new QnUpdateFileInformation(m_target.version, QUrl(urlPrefix + fileName)));
            info->baseFileName = fileName;
            info->fileSize = package.value(lit("size")).toLongLong();
            info->md5 = package.value(lit("md5")).toString();
            m_updateFiles.insert(QnSystemInformation(platform.key(), architecture, modification), info);
        }
    }

    if (!m_target.denyClientUpdates) {
        packages = map.value(lit("clientPackages")).toMap();
        QString arch = QnAppInfo::applicationArch();
        QString modification = QnAppInfo::applicationPlatformModification();
        if (!modification.isEmpty())
            arch += lit("_") + modification;
        QVariantMap package = packages.value(QnAppInfo::applicationPlatform()).toMap().value(arch).toMap();

        if (!package.isEmpty()) {
            QString fileName = package.value(lit("file")).toString();
            m_clientUpdateFile.reset(new QnUpdateFileInformation(m_target.version, QUrl(urlPrefix + fileName)));
            m_clientUpdateFile->baseFileName = fileName;
            m_clientUpdateFile->fileSize = package.value(lit("size")).toLongLong();
            m_clientUpdateFile->md5 = package.value(lit("md5")).toString();
        }

        QnSoftwareVersion minimalVersionToUpdate(map.value(lit("minimalClientVersion")).toString());
        m_clientRequiresInstaller = !minimalVersionToUpdate.isNull() && minimalVersionToUpdate > maximumAvailableVersion();
    } else {
        m_clientRequiresInstaller = true;
    }

    checkUpdateCoverage();
}

void QnCheckForUpdatesPeerTask::at_zipExtractor_finished(int error) {
    QnZipExtractor *zipExtractor = qobject_cast<QnZipExtractor*>(sender());
    if (!zipExtractor)
        return;

    zipExtractor->deleteLater();

    if (error != QnZipExtractor::Ok) {
        finishTask(error == QnZipExtractor::NoFreeSpace ? QnCheckForUpdateResult::NoFreeSpace : QnCheckForUpdateResult::BadUpdateFile);
        return;
    }

    QDir dir = zipExtractor->dir();
    QStringList entries = zipExtractor->fileList();
    foreach (const QString &entry, entries) {
        QString fileName = dir.absoluteFilePath(entry);
        QnSoftwareVersion version;
        QnSystemInformation sysInfo;
        bool isClient;

        if (!verifyUpdatePackage(fileName, &version, &sysInfo, &isClient))
            continue;

        if (m_updateFiles.contains(sysInfo))
            continue;

        if (m_target.version.isNull())
            m_target.version = version;

        if (m_target.version != version) {
            finishTask(QnCheckForUpdateResult::BadUpdateFile);
            return;
        }

        if (isClient) {
            if (m_target.denyClientUpdates)
                continue;

            if (sysInfo != QnSystemInformation::currentSystemInformation())
                continue;
        }

        QnUpdateFileInformationPtr updateFileInformation(new QnUpdateFileInformation(version, fileName));
        QFile file(fileName);
        updateFileInformation->fileSize = file.size();
        updateFileInformation->md5 = makeMd5(&file);
        if (isClient) {
            if (!m_target.denyClientUpdates) {
                m_clientUpdateFile = updateFileInformation;
                QnSoftwareVersion minimalVersion = minimalVersionForUpdatePackage(updateFileInformation->fileName);
                m_clientRequiresInstaller = !minimalVersion.isNull() && minimalVersion > maximumAvailableVersion();
            }
        } else {
            m_updateFiles.insert(sysInfo, updateFileInformation);
        }
    }

    if (!m_clientUpdateFile)
        m_clientRequiresInstaller = true;

    checkUpdateCoverage();
}

void QnCheckForUpdatesPeerTask::finishTask(QnCheckForUpdateResult::Value value) {
    if (m_checkLatestVersion && value == QnCheckForUpdateResult::NoSuchBuild) {
        m_target.version = QnSoftwareVersion();
        value = QnCheckForUpdateResult::NoNewerVersion;
    }

    QnCheckForUpdateResult result(value);
    result.latestVersion = m_target.version;
    result.systems = m_updateFiles.keys().toSet();
    result.clientInstallerRequired = m_clientRequiresInstaller;
    result.releaseNotesUrl = m_releaseNotesUrl;

    emit checkFinished(result);
    finish(static_cast<int>(value));
}

void QnCheckForUpdatesPeerTask::start() {
    base_type::start(m_target.targets);
}

QnUpdateTarget QnCheckForUpdatesPeerTask::target() const {
    return m_target;
}
