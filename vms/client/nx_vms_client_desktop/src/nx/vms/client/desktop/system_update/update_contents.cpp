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
#include <nx/utils/app_info.h>
#include <utils/common/app_info.h>

#include "update_contents.h"

using nx::update::UpdateContents;

namespace {

const QString kPackageIndexFile = "packages.json";
const QString kTempDataDir = "update_temp";
const QString kFilePrefix = "file://";

} // namespace

namespace nx::vms::client::desktop {

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

bool verifyUpdateContents(QnCommonModule* commonModule, nx::update::UpdateContents& contents,
    std::map<QnUuid, QnMediaServerResourcePtr> activeServers)
{
    nx::update::Information& info = contents.info;
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
    if (contents.sourceType == nx::update::UpdateSourceType::file)
    {
        QString uploadDestination = QString("updates/%1/").arg(contents.info.version);
        QList<nx::update::Package> checked;
        for (auto& pkg: contents.info.packages)
        {
            if (contents.sourceType == nx::update::UpdateSourceType::file)
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

    QString errorMessage;
    // Update is allowed if either target version has the same cloud host or
    // there are no servers linked to the cloud in the system.
    QString cloudUrl = nx::network::SocketGlobals::cloud().cloudHost();
    bool boundToCloud = !commonModule->globalSettings()->cloudSystemId().isEmpty();
    bool alreadyInstalled = true;

    QString clientVersionRaw = nx::utils::AppInfo::applicationVersion();
    if (nx::utils::SoftwareVersion(clientVersionRaw) != contents.getVersion())
        alreadyInstalled = false;

    auto systemInfo = QnAppInfo::currentSystemInformation();
    if (nx::update::findPackage(
            systemInfo,
            contents.info,
            true, cloudUrl, boundToCloud, &contents.clientPackage, &errorMessage)
        != nx::update::FindPackageResult::ok)
    {
        NX_ERROR(typeid(UpdateContents))
            << "verifyUpdateManifest(" << contents.info.version
            << ")Error while trying to find client package:" << errorMessage;
    }

    QSet<QnUuid> allServers;

    // We store here a set of packages that should be downloaded manually by the client.
    // We will convert it to a list of values later.
    QSet<nx::update::Package*> manualPackages;
    // Checking if all servers have update packages.
    for (auto record: activeServers)
    {
        auto server = record.second;
        bool isOurServer = !server->hasFlags(Qn::fake_server)
            || helpers::serverBelongsToCurrentSystem(server);
        if (!isOurServer)
            continue;

        auto serverInfo = server->getSystemInfo();
        auto package = findPackage(nx::update::kComponentServer, serverInfo, info);
        if (!package)
        {
            NX_ERROR(typeid(UpdateContents)) << "verifyUpdateManifest server "
                << server->getId()
                << "arch" << serverInfo.arch
                << "platform" << serverInfo.platform
                << "is missing its update package";
            contents.missingUpdate.insert(server->getId());
            continue;
        }

        nx::utils::SoftwareVersion serverVersion = server->getVersion();
        // Prohibiting updates to previous version.
        if (serverVersion > targetVersion)
        {
            NX_ERROR(typeid(UpdateContents)) << "verifyUpdateManifest server "
                << server->getId()
                << "ver" << serverVersion.toString()
                << "is incompatible with this update"
                << "ver" << targetVersion.toString();
            contents.invalidVersion.insert(server->getId());
        }
        else if (serverVersion != targetVersion)
        {
            alreadyInstalled = false;
        }

        if (package)
        {
            package->targets.push_back(server->getId());
            auto hasInternet = server->getServerFlags().testFlag(nx::vms::api::SF_HasPublicIP);
            if (!hasInternet)
                manualPackages.insert(package);
        }

        allServers << record.first;
    }

    for (auto package: manualPackages)
        contents.manualPackages.push_back(*package);

    contents.alreadyInstalled = alreadyInstalled;

    contents.cloudIsCompatible = checkCloudHost(commonModule, targetVersion, contents.info.cloudHost, allServers);

    if (!contents.missingUpdate.empty())
    {
        NX_WARNING(typeid(UpdateContents)) << "verifyUpdateManifest("
            << contents.info.version << ") - detected missing server packages.";
        contents.error = nx::update::InformationError::missingPackageError;
    }

    if (!contents.clientPackage.isValid())
    {
        NX_WARNING(typeid(UpdateContents)) << "verifyUpdateManifest("
            << contents.info.version << ") - detected missing server packages.";
        contents.error = nx::update::InformationError::missingPackageError;
    }

    if (!contents.invalidVersion.empty())
    {
        NX_WARNING(typeid(UpdateContents)) << "verifyUpdateManifest("
            << contents.info.version << ") - detected incompatible version error.";
        contents.error = nx::update::InformationError::incompatibleVersion;
    }

    // Update package has no packages at all.
    if (contents.info.packages.empty() && !activeServers.empty())
    {
        NX_WARNING(typeid(UpdateContents)) << "verifyUpdateManifest("
            << contents.info.version << ") - this update is completely empty.";
        contents.error = nx::update::InformationError::missingPackageError;
    }

    if (!contents.cloudIsCompatible)
    {
        NX_WARNING(typeid(UpdateContents)) << "verifyUpdateManifest("
            << contents.info.version << ") - detected detected incompatible cloud.";
        contents.error = nx::update::InformationError::incompatibleCloudHostError;
    }

    if (!contents.manualPackages.empty())
    {
        QStringList files;
        for (const auto& pkg: contents.manualPackages)
            files.append(pkg.file);

        NX_WARNING(typeid(UpdateContents))
            << "verifyUpdateManifest(" << contents.info.version
            << ") - detected some servers can not download update packages:"
            << contents.info.version, files.join(",");
    }

    contents.verified = true;

    return contents.isValid();
}

} // namespace nx::vms::client::desktop
