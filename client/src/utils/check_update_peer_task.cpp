#include "check_update_peer_task.h"

#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

#include <quazip/quazip.h>
#include <quazip/quazipfile.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <common/common_module.h>
#include <utils/update/update_utils.h>
#include <utils/applauncher_utils.h>

#include "version.h"

namespace {
    const QString buildInformationSuffix = lit("update");
    const QString updateInformationFileName = (lit("update.json"));

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

QnCheckForUpdatesPeerTask::QnCheckForUpdatesPeerTask(QObject *parent) :
    QnNetworkPeerTask(parent),
    m_networkAccessManager(new QNetworkAccessManager(this)),
    m_denyMajorUpdates(true),
    m_disableClientUpdates(false)
{
}

void QnCheckForUpdatesPeerTask::setUpdatesUrl(const QUrl &url) {
    m_updatesUrl = url;
}

void QnCheckForUpdatesPeerTask::setTargetVersion(const QnSoftwareVersion &version) {
    m_targetVersion = version;
    m_updateFileName.clear();
}

QnSoftwareVersion QnCheckForUpdatesPeerTask::targetVersion() const {
    return m_targetVersion;
}

void QnCheckForUpdatesPeerTask::setUpdateFileName(const QString &fileName) {
    m_updateFileName = fileName;
    m_targetVersion = QnSoftwareVersion();
}

QString QnCheckForUpdatesPeerTask::updateFileName() const {
    return m_updateFileName;
}

void QnCheckForUpdatesPeerTask::setDisableClientUpdates(bool f) {
    m_disableClientUpdates = f;
}

bool QnCheckForUpdatesPeerTask::isDisableClientUpdates() const {
    return m_disableClientUpdates;
}

bool QnCheckForUpdatesPeerTask::isClientRequiresInstaller() const {
    return m_clientRequiresInstaller;
}

QHash<QnSystemInformation, QnUpdateFileInformationPtr> QnCheckForUpdatesPeerTask::updateFiles() const {
    return m_updateFiles;
}

QnUpdateFileInformationPtr QnCheckForUpdatesPeerTask::clientUpdateFile() const {
    return m_clientUpdateFile;
}

QString QnCheckForUpdatesPeerTask::temporaryDir() const {
    return m_temporaryUpdateDir;
}

void QnCheckForUpdatesPeerTask::doStart() {
    if (!m_updateFileName.isEmpty())
        checkLocalUpdates();
    else
        checkOnlineUpdates(m_targetVersion);
}

bool QnCheckForUpdatesPeerTask::needUpdate(const QnSoftwareVersion &version, const QnSoftwareVersion &updateVersion) const {
    return (m_targetMustBeNewer && updateVersion > version) || (!m_targetMustBeNewer && updateVersion != version);
}

void QnCheckForUpdatesPeerTask::checkUpdateCoverage() {
    bool needUpdate = false;
    foreach (const QUuid &peerId, peers()) {
        QnMediaServerResourcePtr server = qnResPool->getIncompatibleResourceById(peerId, true).dynamicCast<QnMediaServerResource>();
        if (!server)
            continue;

        QnUpdateFileInformationPtr updateFileInformation = m_updateFiles[server->getSystemInfo()];
        if (!updateFileInformation) {
            cleanUp();
            finish(UpdateImpossible);
            return;
        }
        needUpdate |= this->needUpdate(server->getVersion(), updateFileInformation->version);
    }

    if (!m_disableClientUpdates && !m_clientRequiresInstaller) {
        if (!m_clientUpdateFile) {
            cleanUp();
            finish(UpdateImpossible);
            return;
        }

        if ((m_targetMustBeNewer && m_clientUpdateFile->version > qnCommon->engineVersion()) || (!m_targetMustBeNewer && m_clientUpdateFile->version != qnCommon->engineVersion()))
            needUpdate = true;
    }

    finish(needUpdate ? UpdateFound : NoNewerVersion);
    return;
}

void QnCheckForUpdatesPeerTask::checkBuildOnline() {
    QUrl url(m_updateLocationPrefix + QString::number(m_targetVersion.build()) + lit("/") + buildInformationSuffix);
    QNetworkReply *reply = m_networkAccessManager->get(QNetworkRequest(url));
    connect(reply, &QNetworkReply::finished, this, &QnCheckForUpdatesPeerTask::at_buildReply_finished);
}

void QnCheckForUpdatesPeerTask::checkOnlineUpdates(const QnSoftwareVersion &version) {
    if (m_denyMajorUpdates && !version.isNull()) {
        QnSoftwareVersion currentVersion = qnCommon->engineVersion();
        if (version.major() != currentVersion.major() || version.minor() != currentVersion.minor()) {
            finish(NoSuchBuild);
            return;
        }
    }

    m_updateFiles.clear();
    m_clientUpdateFile.clear();

    m_targetMustBeNewer = version.isNull();
    m_targetVersion = version;

    QNetworkReply *reply = m_networkAccessManager->get(QNetworkRequest(QUrl(m_updatesUrl)));
    connect(reply, &QNetworkReply::finished, this, &QnCheckForUpdatesPeerTask::at_updateReply_finished);
}

void QnCheckForUpdatesPeerTask::checkLocalUpdates() {
    m_updateFiles.clear();
    m_targetMustBeNewer = false;
    m_targetVersion = QnSoftwareVersion();
    m_temporaryUpdateDir.clear();

    if (!QFile::exists(m_updateFileName)) {
        finish(BadUpdateFile);
        return;
    }

    QDir dir = QDir::temp();
    forever {
        QString dirName = QUuid::createUuid().toString();
        if (dir.exists(dirName))
            continue;

        if (!dir.mkdir(dirName)) {
            finish(BadUpdateFile);
            return;
        }

        dir.cd(dirName);
        m_temporaryUpdateDir = dir.absolutePath();
        break;
    }

    if (!extractZipArchive(m_updateFileName, dir)) {
        cleanUp();
        finish(BadUpdateFile);
        return;
    }

    QStringList entries = dir.entryList(QStringList() << lit("*.zip"), QDir::Files);
    foreach (const QString &entry, entries) {
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
            finish(BadUpdateFile);
            return;
        }

        if (isClient) {
            if (m_disableClientUpdates)
                continue;

            if (sysInfo != QnSystemInformation::currentSystemInformation())
                continue;
        }

        QnUpdateFileInformationPtr updateFileInformation(new QnUpdateFileInformation(version, fileName));
        QFile file(fileName);
        updateFileInformation->fileSize = file.size();
        updateFileInformation->md5 = makeMd5(&file);
        if (isClient) {
            m_clientUpdateFile = updateFileInformation;
            QnSoftwareVersion minimalVersion = minimalVersionForUpdatePackage(updateFileInformation->fileName);
            m_clientRequiresInstaller = !minimalVersion.isNull() && minimalVersion > maximumAvailableVersion();
        } else {
            m_updateFiles.insert(sysInfo, updateFileInformation);
        }
    }

    checkUpdateCoverage();
}

void QnCheckForUpdatesPeerTask::cleanUp() {
    if (!m_temporaryUpdateDir.isEmpty()) {
        QDir(m_temporaryUpdateDir).removeRecursively();
        m_temporaryUpdateDir.clear();
    }
}

void QnCheckForUpdatesPeerTask::at_updateReply_finished() {
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        Q_ASSERT_X(0, "This function must be called only from QNetworkReply", Q_FUNC_INFO);
        return;
    }

    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        finish(InternetProblem);
        return;
    }

    QByteArray data = reply->readAll();
    QVariantMap map = QJsonDocument::fromJson(data).toVariant().toMap();
    map = map.value(lit(QN_CUSTOMIZATION_NAME)).toMap();
    QVariantMap releasesMap = map.value(lit("releases")).toMap();

    QString currentRelease;
    if (m_denyMajorUpdates)
        currentRelease = qnCommon->engineVersion().toString(QnSoftwareVersion::MinorFormat);
    else
        currentRelease = map.value(lit("current_release")).toString();

    QnSoftwareVersion latestVersion = QnSoftwareVersion(releasesMap.value(currentRelease).toString());
    QString updatesPrefix = map.value(lit("updates_prefix")).toString();
    if (latestVersion.isNull() || updatesPrefix.isEmpty()) {
        finish(InternetProblem);
        return;
    }

    if (m_targetVersion.isNull())
        m_targetVersion = latestVersion;
    m_updateLocationPrefix = updatesPrefix;

    checkBuildOnline();
}

void QnCheckForUpdatesPeerTask::at_buildReply_finished() {
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        Q_ASSERT_X(0, "This function must be called only from QNetworkReply", Q_FUNC_INFO);
        return;
    }

    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        finish((reply->error() == QNetworkReply::ContentNotFoundError) ? NoSuchBuild : InternetProblem);
        return;
    }

    QByteArray data = reply->readAll();
    QVariantMap map = QJsonDocument::fromJson(data).toVariant().toMap();

    m_targetVersion = QnSoftwareVersion(map.value(lit("version")).toString());

    if (m_targetVersion.isNull()) {
        finish(NoSuchBuild);
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
            QnUpdateFileInformationPtr info(new QnUpdateFileInformation(m_targetVersion, QUrl(urlPrefix + fileName)));
            info->baseFileName = fileName;
            info->fileSize = package.value(lit("size")).toLongLong();
            info->md5 = package.value(lit("md5")).toString();
            m_updateFiles.insert(QnSystemInformation(platform.key(), architecture, modification), info);
        }
    }

    if (!m_disableClientUpdates) {
        packages = map.value(lit("clientPackages")).toMap();
        QString arch = lit(QN_APPLICATION_ARCH);
        QString modification = lit(QN_ARM_BOX);
        if (!modification.isEmpty())
            arch += lit("_") + modification;
        QVariantMap package = packages.value(lit(QN_APPLICATION_PLATFORM)).toMap().value(arch).toMap();

        if (!package.isEmpty()) {
            QString fileName = package.value(lit("file")).toString();
            m_clientUpdateFile.reset(new QnUpdateFileInformation(m_targetVersion, QUrl(urlPrefix + fileName)));
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
