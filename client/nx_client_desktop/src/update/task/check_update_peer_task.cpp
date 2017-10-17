#include "check_update_peer_task.h"

#include <QtCore/QFile>
#include <QtCore/QDir>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>

#include <quazip/quazip.h>
#include <quazip/quazipfile.h>

#include <api/global_settings.h>

#include <common/common_module.h>
#include <common/static_common_module.h>

#include <client/client_settings.h>
#include <client/client_runtime_settings.h>

#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>

#include <utils/update/update_utils.h>
#include <utils/update/zip_utils.h>
#include <utils/applauncher_utils.h>
#include <nx/network/http/async_http_client_reply.h>
#include <nx/fusion/model_functions.h>

#include <utils/common/app_info.h>
#include <nx/utils/log/log.h>

namespace {
    const QString buildInformationSuffix = lit("update.json");
    const QString updateInformationFileName = (lit("update.json"));

    const int httpResponseTimeoutMs = 30000;

    struct CustomizationInfo
    {
        QString current_release;
        QString updates_prefix;
        QString release_notes;
        QString description;
        QMap<QString, QnSoftwareVersion> releases;
    };
    QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
        CustomizationInfo, (json),
        (current_release)(updates_prefix)(release_notes)(description)(releases))

    struct UpdateFileInformation
    {
        QString md5;
        QString file;
        qint64 size;
    };
    QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
        UpdateFileInformation, (json),
        (md5)(file)(size))

    struct BuildInformation
    {
        using PackagesHash = QHash<QString, QHash<QString, UpdateFileInformation>>;
        PackagesHash packages;
        PackagesHash clientPackages;
        QnSoftwareVersion version;
        QString cloudHost;
        QnSoftwareVersion minimalClientVersion;
    };
    QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
        BuildInformation, (json),
        (packages)(clientPackages)(version)(cloudHost)(minimalClientVersion))

    QnSoftwareVersion minimalVersionForUpdatePackage(const QString &fileName) {
        QuaZipFile infoFile(fileName, updateInformationFileName);
        if (!infoFile.open(QuaZipFile::ReadOnly))
            return QnSoftwareVersion();

        QString data = QString::fromUtf8(infoFile.readAll());
        infoFile.close();

        QRegExp minimalVersionRegExp(QLatin1String("\"minimalVersion\"\\s*:\\s*\"([\\d\\.]+)\""));
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
} // namespace

QnCheckForUpdatesPeerTask::QnCheckForUpdatesPeerTask(const QnUpdateTarget &target, QObject *parent) :
    QnNetworkPeerTask(parent),
    m_mainUpdateUrl(nx::utils::Url(qnSettings->updateFeedUrl())),
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

bool QnCheckForUpdatesPeerTask::isUpdateNeed(
    const QnSoftwareVersion& version,
    const QnSoftwareVersion& updateVersion) const
{
    return (m_targetMustBeNewer && updateVersion > version)
        || (!m_targetMustBeNewer && updateVersion != version);
}

void QnCheckForUpdatesPeerTask::checkUpdate()
{
    if (!checkCloudHost())
    {
        finishTask(QnCheckForUpdateResult::IncompatibleCloudHost);
        return;
    }

    finishTask(checkUpdateCoverage());
}

bool QnCheckForUpdatesPeerTask::checkCloudHost()
{
    /* Ignore cloud host for versions lower than 3.0. */
    static const QnSoftwareVersion kCloudRequiredVersion(3, 0);

    if (m_target.version < kCloudRequiredVersion)
        return true;

    /* Update is allowed if either target version has the same cloud host or
       there are no servers linked to the cloud in the system. */
    if (m_cloudHost == nx::network::AppInfo::defaultCloudHost())
        return true;

    const bool isBoundToCloud = !qnGlobalSettings->cloudSystemId().isEmpty();
    if (isBoundToCloud)
        return false;

    const auto serversLinkedToCloud = QnUpdateUtils::getServersLinkedToCloud(peers());
    return serversLinkedToCloud.isEmpty();
}

QnCheckForUpdateResult::Value QnCheckForUpdatesPeerTask::checkUpdateCoverage()
{
    if (m_updateFiles.isEmpty() && m_clientUpdateFile.isNull())
        return QnCheckForUpdateResult::BadUpdateFile;

    bool needUpdate = false;
    for (const auto& peerId: peers())
    {
        const auto server = resourcePool()->getIncompatibleServerById(peerId, true);
        if (!server)
            continue;

        bool updateServer = isUpdateNeed(server->getVersion(), m_target.version);
        if (updateServer && !m_updateFiles.value(server->getSystemInfo()))
        {
            NX_LOG(lit("Update: QnCheckForUpdatesPeerTask: No update file for server [%1 : %2]")
                   .arg(server->getName()).arg(server->getApiUrl().toString()), cl_logDEBUG2);
            return QnCheckForUpdateResult::ServerUpdateImpossible;
        }
        needUpdate |= updateServer;
    }

    if (!m_target.denyClientUpdates && !m_clientRequiresInstaller)
    {
        bool updateClient = isUpdateNeed(qnStaticCommon->engineVersion(), m_target.version);

        if (updateClient && !m_clientUpdateFile)
        {
            NX_LOG(lit("Update: QnCheckForUpdatesPeerTask: No update file for client."), cl_logDEBUG2);
            return QnCheckForUpdateResult::ClientUpdateImpossible;
        }

        needUpdate |= updateClient;
    }

    return needUpdate
        ? QnCheckForUpdateResult::UpdateFound
        : QnCheckForUpdateResult::NoNewerVersion;
}

bool QnCheckForUpdatesPeerTask::isDowngradeAllowed()
{
    if (qnRuntime->isDevMode())
        return true;

    // Check if all server's version is not higher then target. Server downgrade is prohibited.
    return boost::algorithm::all_of(m_target.targets,
        [targetVersion = m_target.version, this]
        (const QnUuid& serverId)
        {
            const auto server = resourcePool()->getIncompatibleServerById(serverId, true);
            if (!server)
                return true;

            const auto status = server->getStatus();

            // Ignore invalid servers, they will not be updated
            if (status == Qn::Offline || status == Qn::Unauthorized)
                return true;

            return server->getVersion() <= targetVersion;
        }
    );
}

void QnCheckForUpdatesPeerTask::checkBuildOnline()
{
    nx::utils::Url url(lit("%1/%2/%3")
        .arg(m_updateLocationPrefix).arg(m_target.version.build()).arg(buildInformationSuffix));

    auto httpClient = nx_http::AsyncHttpClient::create();
    httpClient->setResponseReadTimeoutMs(httpResponseTimeoutMs);
    auto reply = new QnAsyncHttpClientReply(httpClient);
    connect(reply, &QnAsyncHttpClientReply::finished,
        this, &QnCheckForUpdatesPeerTask::at_buildReply_finished);
    NX_LOG(lit("Update: QnCheckForUpdatesPeerTask: Request [%1]").arg(url.toString(QUrl::RemovePassword)), cl_logDEBUG2);
    httpClient->doGet(url);

    m_runningRequests.insert(reply);
}

void QnCheckForUpdatesPeerTask::checkOnlineUpdates()
{
    clear();

    loadServersFromSettings();
    UpdateServerInfo serverInfo;
    serverInfo.url = m_mainUpdateUrl;
    m_updateServers.prepend(serverInfo);

    m_targetMustBeNewer = m_target.version.isNull();
    m_checkLatestVersion = m_target.version.isNull();

    NX_LOG(lit("Update: QnCheckForUpdatesPeerTask: Checking online updates [%1]")
        .arg(m_target.version.toString()), cl_logDEBUG1);

    if (!tryNextServer())
        finishTask(QnCheckForUpdateResult::InternetProblem);
}

void QnCheckForUpdatesPeerTask::checkLocalUpdates()
{
    clear();
    m_targetMustBeNewer = false;

    NX_LOG(lit("Update: QnCheckForUpdatesPeerTask: Checking local update [%1]")
        .arg(m_target.fileName), cl_logDEBUG1);

    if (!QFile::exists(m_target.fileName))
    {
        finishTask(QnCheckForUpdateResult::BadUpdateFile);
        return;
    }

    auto extractor = new QnZipExtractor(m_target.fileName, updatesCacheDir());
    connect(extractor, &QnZipExtractor::finished,
        this, &QnCheckForUpdatesPeerTask::at_zipExtractor_finished);
    extractor->start();
}

void QnCheckForUpdatesPeerTask::at_updateReply_finished(QnAsyncHttpClientReply* reply)
{
    if (!m_runningRequests.remove(reply))
        return;

    reply->deleteLater();

    if (reply->isFailed())
    {
        NX_LOG(lit("Update: QnCheckForUpdatesPeerTask: Request to %1 failed.")
            .arg(reply->url().toString()), cl_logDEBUG2);

        if (!tryNextServer())
            finishTask(QnCheckForUpdateResult::InternetProblem);

        return;
    }

    NX_LOG(lit("Update: QnCheckForUpdatesPeerTask: Reply received from %1.")
        .arg(reply->url().toString()), cl_logDEBUG2);

    const auto json = QJsonDocument::fromJson(reply->data()).object();

    if (m_currentUpdateUrl == m_mainUpdateUrl)
    {
        const auto it = json.find(lit("__info"));
        if (it != json.end())
        {
            qnSettings->setAlternativeUpdateServers(it.value().toArray().toVariantList());
            qnSettings->save();
            loadServersFromSettings();
        }
    }

    CustomizationInfo customizationInfo;
    if (!QJson::deserialize(json, QnAppInfo::customizationName(), &customizationInfo))
    {
        if (!tryNextServer())
            finishTask(QnCheckForUpdateResult::NoSuchBuild);
        return;
    }

    QString currentRelease = customizationInfo.current_release;
    if (QnSoftwareVersion(currentRelease) < qnStaticCommon->engineVersion())
        currentRelease = qnStaticCommon->engineVersion().toString(QnSoftwareVersion::MinorFormat);

    const auto latestVersion = customizationInfo.releases[currentRelease];
    const QString updatesPrefix = customizationInfo.updates_prefix;

    if ((m_target.version.build() == 0 && latestVersion.isNull()) || updatesPrefix.isEmpty())
    {
        if (!tryNextServer())
            finishTask(QnCheckForUpdateResult::NoSuchBuild);
        return;
    }

    if (m_target.version.isNull())
    {
        m_target.version = latestVersion;
    }
    else if (m_target.version.build() == 0)
    {
        m_target.version = customizationInfo.releases[
            m_target.version.toString(QnSoftwareVersion::MinorFormat)];
    }

    m_updateLocationPrefix = updatesPrefix;
    m_releaseNotesUrl = customizationInfo.release_notes;
    m_description = customizationInfo.description;

    checkBuildOnline();
}

void QnCheckForUpdatesPeerTask::at_buildReply_finished(QnAsyncHttpClientReply* reply)
{
    if (!m_runningRequests.remove(reply))
        return;

    reply->deleteLater();

    if (reply->isFailed() || (reply->response().statusLine.statusCode != nx_http::StatusCode::ok))
    {
        NX_LOG(lit("Update: QnCheckForUpdatesPeerTask: Request to %1 failed.")
            .arg(reply->url().toString()), cl_logDEBUG2);

        if (!tryNextServer())
            finishTask(QnCheckForUpdateResult::NoSuchBuild);

        return;
    }

    NX_LOG(lit("Update: QnCheckForUpdatesPeerTask: Reply received from %1.")
        .arg(reply->url().toString()), cl_logDEBUG2);

    BuildInformation buildInformation;
    if (!QJson::deserialize(reply->data(), &buildInformation) || buildInformation.version.isNull())
    {
        if (!tryNextServer())
        {
            NX_LOG(lit("Update: QnCheckForUpdatesPeerTask: No such build."), cl_logDEBUG1);
            finishTask(QnCheckForUpdateResult::NoSuchBuild);
        }
        return;
    }

    m_target.version = buildInformation.version;

    if (m_target.version.isNull())
    {
        if (!tryNextServer())
        {
            NX_LOG(lit("Update: QnCheckForUpdatesPeerTask: No such build."), cl_logDEBUG1);
            finishTask(QnCheckForUpdateResult::NoSuchBuild);
        }
        return;
    }

    m_cloudHost = buildInformation.cloudHost;

    if (!isDowngradeAllowed())
    {
        finishTask(QnCheckForUpdateResult::DowngradeIsProhibited);
        return;
    }

    const auto urlPrefix = lit("%1/%2/").arg(m_updateLocationPrefix).arg(m_target.version.build());

    for (auto platform = buildInformation.packages.begin();
        platform != buildInformation.packages.end();
        ++platform)
    {
        const auto& variants = platform.value();
        for (auto variant = variants.begin(); variant != variants.end(); ++variant)
        {
            // We suppose arch name does not contain '_' char.
            // E.g. arm_edge1_2 will be split to "arm" and "edge1_2".
            const auto arch = variant.key().section(L'_', 0, 0);
            const auto modification = variant.key().mid(arch.size() + 1);

            QnUpdateFileInformationPtr info(
                new QnUpdateFileInformation(m_target.version, nx::utils::Url(urlPrefix + variant->file)));
            info->baseFileName = variant->file;
            info->fileSize = variant->size;
            info->md5 = variant->md5;
            QnSystemInformation systemInformation(platform.key(), arch, modification);
            m_updateFiles.insert(systemInformation, info);

            NX_LOG(
                lit("Update: QnCheckForUpdatesPeerTask: Server update file [%1, %2, %3].")
                   .arg(systemInformation.toString())
                   .arg(info->baseFileName)
                   .arg(info->fileSize),
                cl_logDEBUG1);
        }
    }

    if (!m_target.denyClientUpdates)
    {
        const auto modification = QnAppInfo::applicationPlatformModification();
        auto variantKey = QnAppInfo::applicationArch();
        if (!modification.isEmpty())
            variantKey += L'_' + modification;
        const auto variant =
            buildInformation.clientPackages[QnAppInfo::applicationPlatform()][variantKey];

        if (!variant.file.isEmpty())
        {
            m_clientUpdateFile.reset(
                new QnUpdateFileInformation(m_target.version, nx::utils::Url(urlPrefix + variant.file)));
            m_clientUpdateFile->baseFileName = variant.file;
            m_clientUpdateFile->fileSize = variant.size;
            m_clientUpdateFile->md5 = variant.md5;

            NX_LOG(
                lit("Update: QnCheckForUpdatesPeerTask: Client update file [%2, %3].")
                   .arg(m_clientUpdateFile->baseFileName)
                   .arg(m_clientUpdateFile->fileSize),
                cl_logDEBUG1);
        }

        QnSoftwareVersion minimalVersionToUpdate = buildInformation.minimalClientVersion;
        m_clientRequiresInstaller = !minimalVersionToUpdate.isNull()
            && minimalVersionToUpdate > maximumAvailableVersion();
    }
    else
    {
        m_clientRequiresInstaller = true;
    }
    NX_LOG(lit("Update: QnCheckForUpdatesPeerTask: Client requires installer [%1].")
        .arg(m_clientRequiresInstaller), cl_logDEBUG1);

    checkUpdate();
}

void QnCheckForUpdatesPeerTask::at_zipExtractor_finished(int error)
{
    auto zipExtractor = qobject_cast<QnZipExtractor*>(sender());
    if (!zipExtractor)
        return;

    zipExtractor->deleteLater();

    if (error != QnZipExtractor::Ok)
    {
        finishTask(error == QnZipExtractor::NoFreeSpace
            ? QnCheckForUpdateResult::NoFreeSpace
            : QnCheckForUpdateResult::BadUpdateFile);
        return;
    }

    const auto dir = zipExtractor->dir();
    for (const auto& entry: zipExtractor->fileList())
    {
        QString fileName = dir.absoluteFilePath(entry);
        QnSoftwareVersion version;
        QnSystemInformation sysInfo;
        QString cloudHost;
        bool isClient = false;

        if (!verifyUpdatePackage(fileName, &version, &sysInfo, &cloudHost, &isClient))
            continue;

        if (m_updateFiles.contains(sysInfo))
            continue;

        if (m_target.version.isNull())
            m_target.version = version;

        if (m_target.version != version)
        {
            finishTask(QnCheckForUpdateResult::BadUpdateFile);
            return;
        }

        m_cloudHost = cloudHost;

        if (isClient)
        {
            if (m_target.denyClientUpdates)
                continue;

            if (sysInfo != QnSystemInformation::currentSystemInformation())
                continue;
        }

        QFile file(fileName);

        QnUpdateFileInformationPtr updateFileInformation(
            new QnUpdateFileInformation(version, fileName));
        updateFileInformation->fileSize = file.size();
        updateFileInformation->md5 = makeMd5(&file);

        if (isClient)
        {
            if (!m_target.denyClientUpdates)
            {
                m_clientUpdateFile = updateFileInformation;
                const auto minimalVersion =
                    minimalVersionForUpdatePackage(updateFileInformation->fileName);
                m_clientRequiresInstaller =
                    !minimalVersion.isNull() && minimalVersion > maximumAvailableVersion();
            }
        }
        else
        {
            m_updateFiles.insert(sysInfo, updateFileInformation);
        }
    }

    if (!m_clientUpdateFile)
        m_clientRequiresInstaller = true;

    checkUpdate();
}

void QnCheckForUpdatesPeerTask::finishTask(QnCheckForUpdateResult::Value value)
{
    if (m_checkLatestVersion && value == QnCheckForUpdateResult::NoSuchBuild)
    {
        m_target.version = QnSoftwareVersion();
        value = QnCheckForUpdateResult::NoNewerVersion;
    }

    QnCheckForUpdateResult result(value);
    result.version = m_target.version;
    result.systems = m_updateFiles.keys().toSet();
    result.clientInstallerRequired = m_clientRequiresInstaller;
    result.releaseNotesUrl = m_releaseNotesUrl;
    result.description = m_description;
    result.cloudHost = m_cloudHost;

    NX_LOG(lit("Update: QnCheckForUpdatesPeerTask: Check finished [%1, %2].")
       .arg(value).arg(result.version.toString()), cl_logDEBUG1);

    emit checkFinished(result);
    finish(static_cast<int>(value));
}

void QnCheckForUpdatesPeerTask::loadServersFromSettings()
{
    m_updateServers.clear();

    for (const auto& alternative: qnSettings->alternativeUpdateServers())
    {
        const auto infoMap = alternative.toMap();

        UpdateServerInfo serverInfo;
        serverInfo.url = nx::utils::Url::fromQUrl(infoMap.value(lit("url")).toUrl());
        if (!serverInfo.url.isValid())
            continue;

        m_updateServers.append(serverInfo);
    }
}

bool QnCheckForUpdatesPeerTask::tryNextServer()
{
    if (m_updateServers.isEmpty())
        return false;

    const auto serverInfo = m_updateServers.takeFirst();
    m_currentUpdateUrl = serverInfo.url;

    auto httpClient = nx_http::AsyncHttpClient::create();
    httpClient->setResponseReadTimeoutMs(httpResponseTimeoutMs);
    auto reply = new QnAsyncHttpClientReply(httpClient);
    connect(reply, &QnAsyncHttpClientReply::finished,
        this, &QnCheckForUpdatesPeerTask::at_updateReply_finished);
    NX_LOG(lit("Update: QnCheckForUpdatesPeerTask: Request [%1]")
        .arg(m_currentUpdateUrl.toString(QUrl::RemovePassword)), cl_logDEBUG2);
    httpClient->doGet(m_currentUpdateUrl);
    m_runningRequests.insert(reply);
    return true;
}

void QnCheckForUpdatesPeerTask::clear()
{
    m_updateFiles.clear();
    m_clientUpdateFile.clear();
    m_releaseNotesUrl.clear();
    m_description.clear();
}

void QnCheckForUpdatesPeerTask::start()
{
    base_type::start(m_target.targets);
}

QnUpdateTarget QnCheckForUpdatesPeerTask::target() const
{
    return m_target;
}

void QnCheckForUpdatesPeerTask::doCancel()
{
    m_runningRequests.clear();
}
