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

void ClientVerificationData::fillDefault()
{
    systemInfo = QnAppInfo::currentSystemInformation();
    currentVersion = nx::utils::SoftwareVersion(nx::utils::AppInfo::applicationVersion());
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

nx::update::Package* findPackageForOsVariant(
    const QList<nx::update::Package*>& packages,
    const QString& component,
    const QString& variantVersion)
{
    nx::utils::SoftwareVersion version{variantVersion};
    for (auto& package: packages)
    {
        if (package == nullptr)
            continue;

        if (component != package->component)
            continue;

        if (package->variantVersion.isEmpty())
            return package;

        if (nx::utils::SoftwareVersion(package->variantVersion) < version)
            return package;
    }
    return nullptr;
}

QString UpdateStrings::getReportForUnsupportedServer(const nx::vms::api::SystemInformation& info)
{
    QString platform;
    QString version = info.version;

    if (info.platform == "linux")
    {
        if (info.modification == "ubuntu")
            platform = "Ubuntu";
        else
            return tr("This Linux platform is no longer supported"); //< Note "platform" wording.
    }
    else if (info.platform == "windows")
    {
        platform = "Windows";
        version.clear();

        const nx::utils::SoftwareVersion ver(info.version);
        if (ver.major() == 6)
        {
            switch (ver.minor())
            {
                case 0: version = "Vista"; break;
                case 1: version = "7"; break;
                case 2: version = "8"; break;
                case 3: version = "8.1"; break;
                default: break;
            }
        }
        else if (ver.major() == 10)
        {
            version = "10";
        }
    }
    else if (info.modification == "mac")
    {
        platform = "Mac OS X";
    }
    else
    {
        return tr("This OS version is no longer supported");
    }

    if (version.isEmpty())
    {
        return tr("This %1 version is no longer supported", "%1 is OS name, e.g. Windows")
            .arg(platform);
    }
    else
    {
        return tr("%1 %2 is no longer supported", "%1 %2 are OS name and version, e.g. Windows 7")
            .arg(platform, version);
    }
}

bool verifyUpdateContents(QnCommonModule* commonModule, nx::update::UpdateContents& contents,
    std::map<QnUuid, QnMediaServerResourcePtr> activeServers,
    const ClientVerificationData& clientData)
{
    nx::update::Information& info = contents.info;
    if (contents.error != nx::update::InformationError::noError)
    {
        NX_ERROR(typeid(UpdateContents)) << "verifyUpdateManifest("
            << contents.info.version << ") has an error before verification:"
            << nx::update::toString(contents.error);
        return false;
    }

    if (contents.info.isEmpty())
    {
        contents.error = nx::update::InformationError::missingPackageError;
        return false;
    }

    nx::utils::SoftwareVersion targetVersion(info.version);

    contents.invalidVersion.clear();
    contents.missingUpdate.clear();

    // Check if some packages from manifest do not exist.
    if (contents.sourceType == nx::update::UpdateSourceType::file && !contents.packagesGenerated)
    {
        contents.filesToUpload.clear();
        QString uploadDestination = QString("updates/%1/").arg(contents.info.version);
        QList<nx::update::Package> checked;
        for (auto& pkg: contents.info.packages)
        {
            if (contents.sourceType == nx::update::UpdateSourceType::file)
            {
                QFileInfo file(contents.storageDir.filePath(pkg.file));
                if (file.exists())
                {
                    // In 4.0 we should upload only server components. There is no way to make
                    // sure server keeps client packages.
                    if (pkg.isServer())
                        contents.filesToUpload.push_back(pkg.file);
                    pkg.localFile = file.fileName();
                    // This filename will be used by uploader.
                    pkg.file = uploadDestination + file.fileName();
                    checked.push_back(pkg);
                }
                else
                {
                    NX_ERROR(typeid(UpdateContents)) << " missing update file" << pkg.file;
                }
            }
        }
        contents.packagesGenerated = true;
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

    /**
     * Maps system information to a set of packages. We use this cache to find and check
     * if a specific OS variant is supported.
     */

    using PackageCache = std::map<nx::vms::api::SystemInformation, QList<nx::update::Package*>,
        nx::update::SystemInformationComparator>;
    PackageCache serverPackageCache;
    PackageCache clientPackageCache;

    for (auto& package: info.packages)
    {
        nx::vms::api::SystemInformation info;
        info.arch = package.arch;
        info.modification = package.variant;
        info.platform = package.platform;

        auto& cache = package.isClient()
            ? clientPackageCache
            : serverPackageCache;

        if (!cache.count(info))
            cache.emplace(info, QList<nx::update::Package*>());
        auto& record = cache[info];
        record.append(&package);
    }

    // We will try to set it to false when we check servers and clients
    bool alreadyInstalled = true;

    if (!clientData.clientId.isNull())
    {
        if (clientData.currentVersion != contents.getVersion())
            alreadyInstalled = false;

        auto it = clientPackageCache.find(clientData.systemInfo);
        if (it == clientPackageCache.end())
        {
            NX_ERROR(typeid(UpdateContents)) << "verifyUpdateManifest("
                << contents.info.version << ") client arch"
                << clientData.systemInfo.arch
                << "platform" << clientData.systemInfo.platform
                << "variant" << clientData.systemInfo.modification
                << "is missing its update package";
            contents.missingUpdate.insert(clientData.clientId);
        }
        else
        {
            auto& packageVariants = it->second;

            auto package = findPackageForOsVariant(
                packageVariants, nx::update::kComponentClient, clientData.systemInfo.version);

            if (!package)
            {
                NX_ERROR(typeid(UpdateContents))
                    << "verifyUpdateManifest(" << contents.info.version
                    << ") current client OS version is unsupported";
                QString message = UpdateStrings::getReportForUnsupportedServer(clientData.systemInfo);
                contents.unsuportedSystemsReport.insert(clientData.clientId, message);
            }
            else
            {
                contents.clientPackage = *package;
            }
        }
    }

    QSet<QnUuid> allServers;

    // We store here a set of packages that should be downloaded manually by the client.
    // We will convert it to a list of values later.
    QSet<nx::update::Package*> manualPackages;

    QSet<QnUuid> serversWithNewerVersion;

    // TODO: Should reverse this verification: get all platform configurations and find packages for them.
    // Checking if all servers have update packages.
    for (auto record: activeServers)
    {
        auto server = record.second;
        bool isOurServer = !server->hasFlags(Qn::fake_server)
            || helpers::serverBelongsToCurrentSystem(server);
        if (!isOurServer)
            continue;

        auto serverInfo = server->getSystemInfo();

        auto it = serverPackageCache.find(serverInfo);
        if (it == serverPackageCache.end())
        {
            NX_ERROR(typeid(UpdateContents)) << "verifyUpdateManifest("
                << contents.info.version << ") server"
                << server->getId()
                << "arch" << serverInfo.arch
                << "platform" << serverInfo.platform
                << "variant" << serverInfo.modification
                << "is missing its update package";
            contents.missingUpdate.insert(server->getId());
            continue;
        }

        auto& packageVariants = it->second;

        auto package = findPackageForOsVariant(packageVariants, nx::update::kComponentServer, serverInfo.version);

        if (!package)
        {
            NX_ERROR(typeid(UpdateContents))
                << "verifyUpdateManifest(" << contents.info.version << ") server"
                << server->getId() << " has unsupported os =" << serverInfo.modification
                << "osversion =" << serverInfo.version;
            QString message = UpdateStrings::getReportForUnsupportedServer(serverInfo);
            contents.unsuportedSystemsReport.insert(server->getId(), message);
        }

        nx::utils::SoftwareVersion serverVersion = server->getVersion();
        // Prohibiting updates to previous version.
        if (serverVersion > targetVersion)
        {
            NX_WARNING(typeid(UpdateContents)) << "verifyUpdateManifest("
                << contents.info.version << ") server"
                << server->getId()
                << "ver" << serverVersion.toString()
                << "has a newer version";
            serversWithNewerVersion.insert(server->getId());
        }
        else if (serverVersion != targetVersion)
        {
            alreadyInstalled = false;
        }

        if (package && serverVersion <= targetVersion)
        {
            package->targets.insert(server->getId());
            contents.serversWithUpdate.insert(server->getId());
            auto hasInternet = server->getServerFlags().testFlag(nx::vms::api::SF_HasPublicIP);
            if (!hasInternet)
                manualPackages.insert(package);
        }

        allServers << record.first;
    }

    for (auto package: manualPackages)
        contents.manualPackages.push_back(*package);

    if (contents.serversWithUpdate.empty())
        contents.invalidVersion.unite(serversWithNewerVersion);
    else
        contents.ignorePeers = serversWithNewerVersion;

    contents.alreadyInstalled = alreadyInstalled;
    if (commonModule)
    {
        contents.cloudIsCompatible = checkCloudHost(
            commonModule, targetVersion, contents.info.cloudHost, allServers);
    }

    for (auto id: contents.unsuportedSystemsReport.keys())
        contents.missingUpdate.remove(id);

    if (!contents.missingUpdate.empty() || !contents.unsuportedSystemsReport.empty())
    {
        NX_WARNING(typeid(UpdateContents)) << "verifyUpdateManifest("
            << contents.info.version << ") - detected missing packages.";
        contents.error = nx::update::InformationError::missingPackageError;
    }

    if (!clientData.clientId.isNull()
        && (contents.missingUpdate.contains(clientData.clientId) || !contents.clientPackage.isValid()))
    {
        if (targetVersion > clientData.currentVersion
            && !clientData.installedVersions.count(targetVersion))
        {
            NX_WARNING(typeid(UpdateContents)) << "verifyUpdateManifest("
                << contents.info.version << ") - we should install client package, but there are no such.";
            contents.error = nx::update::InformationError::missingPackageError;
        }
        else
        {
            NX_WARNING(typeid(UpdateContents)) << "verifyUpdateManifest("
                << contents.info.version << ") - there are no client package, but applauncher has it. That's ok.";
        }
        contents.missingUpdate.insert(clientData.clientId);
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
        contents.error = nx::update::InformationError::incompatibleCloudHost;
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

    return contents.isValidToInstall();
}

} // namespace nx::vms::client::desktop
