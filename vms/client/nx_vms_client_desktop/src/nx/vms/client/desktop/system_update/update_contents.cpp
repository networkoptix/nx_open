#include <api/global_settings.h>
#include <common/common_module.h>
#include <core/resource_management/resource_pool.h>
#include <core/resource/media_server_resource.h>
#include <client_core/client_core_module.h>
#include <client/client_settings.h>
#include <network/system_helpers.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <nx/update/update_check.h>
#include <nx/vms/discovery/manager.h>
#include <utils/common/app_info.h>

#include "update_contents.h"

namespace {

const QString kPackageIndexFile = "packages.json";
const QString kTempDataDir = "update_temp";
const QString kFilePrefix = "file://";

} // namespace

namespace nx::vms::client::desktop {

nx::utils::SoftwareVersion UpdateContents::getVersion() const
{
    return nx::utils::SoftwareVersion(info.version);
}

// Check if we can apply this update.
bool UpdateContents::isValid() const
{
    return missingUpdate.empty()
        && !info.version.isEmpty()
        && invalidVersion.empty()
        && clientPackage.isValid()
        && error == nx::update::InformationError::noError;
}

nx::update::Package findClientPackage(const nx::update::Information& updateInfo)
{
    // Find client package.
    nx::update::Package result;
    const auto modification = QnAppInfo::applicationPlatformModification();
    auto arch = QnAppInfo::applicationArch();

    for (auto& pkg: updateInfo.packages)
    {
        if (pkg.component == nx::update::kComponentClient)
        {
            // Check arch and OS
            if (pkg.arch == arch && pkg.variant == modification)
            {
                result = pkg;
                break;
            }
        }
    }

    return result;
}

const nx::update::Package* findPackageForServer(
    QnMediaServerResourcePtr server, const nx::update::Information& info)
{
    auto serverInfo = server->getSystemInfo();

    for(const auto& pkg: info.packages)
    {
        if (pkg.component == nx::update::kComponentServer)
        {
            // Check arch and OS
            if (pkg.arch == serverInfo.arch
                && pkg.platform == serverInfo.platform
                && pkg.variant == serverInfo.modification)
                return &pkg;
        }
    }
    return nullptr;
}

QSet<QnUuid> getServersLinkedToCloud(QnCommonModule* commonModule, const QSet<QnUuid>& peers)
{
    QSet<QnUuid> result;

    const auto moduleManager = commonModule->moduleDiscoveryManager();
    if (!moduleManager)
        return result;

    for (const auto& id: peers)
    {
        const auto server = commonModule->resourcePool()->getIncompatibleServerById(id);
        if (!server)
            continue;

        const auto module = moduleManager->getModule(id);
        if (module && !module->cloudSystemId.isEmpty())
            result.insert(id);
    }

    return result;
}

bool checkCloudHost(QnCommonModule* commonModule, nx::utils::SoftwareVersion targetVersion, QString cloudUrl, const QSet<QnUuid>& peers)
{
    NX_ASSERT(commonModule);
    /* Ignore cloud host for versions lower than 3.0. */
    static const nx::utils::SoftwareVersion kCloudRequiredVersion(3, 0);

    if (targetVersion < kCloudRequiredVersion)
        return true;

    /* Update is allowed if either target version has the same cloud host or
       there are no servers linked to the cloud in the system. */
    if (cloudUrl == nx::network::SocketGlobals::cloud().cloudHost())
        return true;

    const bool isBoundToCloud = !commonModule->globalSettings()->cloudSystemId().isEmpty();
    if (isBoundToCloud)
        return false;

    const auto serversLinkedToCloud = getServersLinkedToCloud(commonModule, peers);
    return serversLinkedToCloud.isEmpty();
}

bool verifyUpdateContents(QnCommonModule* commonModule, UpdateContents& contents,
    std::map<QnUuid, QnMediaServerResourcePtr> activeServers)
{
    const nx::update::Information& info = contents.info;
    if (contents.error != nx::update::InformationError::noError)
        return false;
    // Hack to prevent double verification of update package.
    if (contents.verified)
        return contents.error != nx::update::InformationError::noError;

    nx::utils::SoftwareVersion targetVersion(info.version);

    contents.invalidVersion.clear();
    contents.missingUpdate.clear();
    contents.filesToUpload.clear();

    // Check if some packages from manifest do not exist.
    if (contents.sourceType == UpdateSourceType::file)
    {
        QString uploadDestination = QString("updates/%1/").arg(contents.info.version);
        QList<nx::update::Package> checked;
        for(auto& pkg: contents.info.packages)
        {
            if (contents.sourceType == UpdateSourceType::file)
            {
                QFileInfo file(contents.storageDir.filePath(pkg.file));
                if (file.exists())
                {
                    contents.filesToUpload.push_back(pkg.file);
                    pkg.url = "";
                    pkg.file = uploadDestination + file.fileName();
                    checked.push_back(pkg);
                }
                else
                {
                    NX_ERROR(typeid(UpdateContents)) << " missing update file" << pkg.file;
                }
            }
        }
        contents.info.packages = checked;
    }

    if (contents.info.eulaLink.startsWith(kFilePrefix))
    {
        // Need to adjust eula link to absolute path to this files.
        QString eulaLocalPath = contents.info.eulaLink.mid(kFilePrefix.size());
        contents.eulaPath = kFilePrefix + contents.storageDir.filePath(eulaLocalPath);
    }
    else
    {
        // TODO: Should get EULA file
        contents.eulaPath = contents.info.eulaLink;
    }

    contents.clientPackage = findClientPackage(contents.info);

    QSet<QnUuid> allServers;
    // Checking if all servers have update packages.
    for(auto record: activeServers)
    {
        auto server = record.second;
        bool isOurServer = !server->hasFlags(Qn::fake_server)
            || helpers::serverBelongsToCurrentSystem(server);
        if (!isOurServer)
            continue;
        auto serverInfo = server->getSystemInfo();

        auto package = findPackageForServer(server, info);
        if (!package)
        {
            NX_ERROR(typeid(UpdateContents)) << "verifyUpdateManifest server "
                << server->getId()
                << "arch" << serverInfo.arch
                << "platform" << serverInfo.platform
                << "is missing its update package";
            contents.missingUpdate.insert(server);
        }

        nx::utils::SoftwareVersion serverVersion = server->getVersion();
        // Prohibiting updates to previous version
        if (serverVersion > targetVersion)
        {
            NX_ERROR(typeid(UpdateContents)) << "verifyUpdateManifest server "
                << server->getId()
                << "ver" << serverVersion.toString()
                << "is incompatible with this update"
                << "ver" << targetVersion.toString();
            contents.invalidVersion.insert(server);
        }

        allServers << record.first;
    }

    contents.cloudIsCompatible = checkCloudHost(commonModule, targetVersion, contents.info.cloudHost, allServers);

    if (!contents.missingUpdate.empty() || !contents.clientPackage.isValid())
    {
        NX_WARNING(typeid(UpdateContents)) << "verifyUpdateManifest(" << contents.info.version <<") - detected missing packages";
        contents.error = nx::update::InformationError::missingPackageError;
    }

    if (!contents.invalidVersion.empty())
    {
        NX_WARNING(typeid(UpdateContents)) << "verifyUpdateManifest(" << contents.info.version <<") - detected incompatible version error";
        contents.error = nx::update::InformationError::incompatibleVersion;
    }

    // Update package has no packages at all.
    if (contents.info.packages.empty())
    {
        NX_WARNING(typeid(UpdateContents)) << "verifyUpdateManifest(" << contents.info.version <<") - this update is completely empty";
        contents.error = nx::update::InformationError::missingPackageError;
    }

    if (!contents.cloudIsCompatible)
    {
        NX_WARNING(typeid(UpdateContents)) << "verifyUpdateManifest(" << contents.info.version <<") - detected incompatible cloud";
        contents.error = nx::update::InformationError::incompatibleCloudHostError;
    }

    contents.verified = true;

    return contents.isValid();
}

std::future<UpdateContents> checkLatestUpdate()
{
    QString updateUrl = qnSettings->updateFeedUrl();
    return std::async(std::launch::async,
        [updateUrl]()
        {
            UpdateContents result;
            result.info = nx::update::updateInformation(updateUrl, nx::update::kLatestVersion, &result.error);
            result.sourceType = UpdateSourceType::internet;
            result.source = lit("%1 for build=%2").arg(updateUrl, nx::update::kLatestVersion);
            return result;
        });
}

std::future<UpdateContents> checkSpecificChangeset(QString build)
{
    QString updateUrl = qnSettings->updateFeedUrl();

    return std::async(std::launch::async,
        [updateUrl, build]()
        {
            UpdateContents result;
            result.info = nx::update::updateInformation(updateUrl, build, &result.error);
            result.sourceType = UpdateSourceType::internetSpecific;
            result.source = lit("%1 for build=%2").arg(updateUrl, build);
            return result;
        });
}


} // namespace nx::vms::client::desktop
