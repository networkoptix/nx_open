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

#include "update_verification.h"

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
    osInfo = nx::utils::OsInfo::current();
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

QString UpdateStrings::getReportForUnsupportedOs(const nx::utils::OsInfo& info)
{
    QString platform;
    QString version = info.variantVersion;

    if (info.platform.startsWith("linux"))
    {
        if (info.variant == "ubuntu")
            platform = "Ubuntu";
        else
            return tr("This Linux platform is no longer supported"); //< Note "platform" wording.
    }
    else if (info.platform.startsWith("windows"))
    {
        platform = "Windows";
        version.clear();

        const nx::utils::SoftwareVersion ver(info.variantVersion);
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
    else if (info.platform == "macos")
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

bool verifyUpdateContents(
    QnCommonModule* commonModule,
    nx::update::UpdateContents& contents,
    const std::map<QnUuid, QnMediaServerResourcePtr>& activeServers,
    const ClientVerificationData& clientData)
{
    nx::update::Information& info = contents.info;
    if (contents.error != nx::update::InformationError::noError)
    {
        NX_ERROR(typeid(UpdateContents)) << "verifyUpdateContents("
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

    if (contents.info.version.isEmpty() || targetVersion.isNull())
    {
        NX_ERROR(typeid(UpdateContents),
            "verifyUpdateContents(...) - missing or invalid version.");
        contents.error = nx::update::InformationError::jsonError;
        return false;
    }

    contents.invalidVersion.clear();
    contents.missingUpdate.clear();
    contents.unsuportedSystemsReport.clear();

    // Check if some packages from manifest do not exist.
    if (contents.sourceType == nx::update::UpdateSourceType::file && !contents.packagesGenerated)
    {
        contents.filesToUpload.clear();
        QString uploadDestination = QString("updates/%1/").arg(targetVersion.build());
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

    // We will try to set it to false when we check servers and clients.
    contents.alreadyInstalled = true;

    if (!clientData.clientId.isNull()
        && (clientData.currentVersion < targetVersion
            || (clientData.currentVersion > targetVersion && clientData.compatibilityMode))
        && clientData.installedVersions.find(targetVersion) == clientData.installedVersions.end())
    {
        contents.alreadyInstalled = false;

        auto [result, package] = update::findPackageForVariant(info, true, clientData.osInfo);
        switch (result)
        {
            case update::FindPackageResult::ok:
                contents.clientPackage = *package;
                contents.peersWithUpdate.insert(clientData.clientId);
                break;

            case update::FindPackageResult::osVersionNotSupported:
                NX_ERROR(typeid(UpdateContents), "verifyUpdateManifest(%1) OS version is not supported: %2",
                    contents.info.version, clientData.osInfo);
                contents.unsuportedSystemsReport.insert(
                    clientData.clientId,
                    UpdateStrings::getReportForUnsupportedOs(clientData.osInfo));
                break;

            case update::FindPackageResult::otherError:
            default:
                NX_ERROR(typeid(UpdateContents), "verifyUpdateManifest(%1) Update package is missing for %2",
                    contents.info.version, clientData.osInfo);
                contents.missingUpdate.insert(clientData.clientId);
                break;
        }
    }

    QHash<nx::utils::OsInfo, update::Package*> serverPackages;

    QSet<QnUuid> allServers;

    // We store here a set of packages that should be downloaded manually by the client.
    // We will convert it to a list of values later.
    QSet<nx::update::Package*> manualPackages;

    QSet<QnUuid> serversWithNewerVersion;

    // TODO: Should reverse this verification: get all platform configurations and find packages for them.
    // Checking if all servers have update packages.
    for (auto& [id, server]: activeServers)
    {
        bool isOurServer = !server->hasFlags(Qn::fake_server)
            || helpers::serverBelongsToCurrentSystem(server);
        if (!isOurServer)
            continue;

        allServers.insert(id);

        const auto& osInfo = server->getOsInfo();
        if (!osInfo.isValid())
        {
            NX_WARNING(typeid(UpdateContents),
                "verifyUpdateManifest(%1) - server %2 has invalid osInfo",
                contents.info.version, id);
        }

        const nx::utils::SoftwareVersion serverVersion = server->getVersion();
        // Prohibiting updates to previous version.
        if (serverVersion >= targetVersion)
        {
            NX_WARNING(typeid(UpdateContents), "verifyUpdateManifest(%1) Server %2 has a newer version %3",
                contents.info.version, id, serverVersion);
            serversWithNewerVersion.insert(id);
            continue;
        }

        update::Package* package = serverPackages[osInfo];
        if (!package)
        {
            auto [result, foundPackage] = update::findPackageForVariant(info, false, osInfo);

            switch (result)
            {
                case update::FindPackageResult::ok:
                    package = foundPackage;
                    serverPackages[osInfo] = package;
                    break;

                case update::FindPackageResult::osVersionNotSupported:
                    NX_ERROR(typeid(UpdateContents),
                        "verifyUpdateManifest(%1) OS version is not supported: %2",
                        contents.info.version, osInfo);
                    contents.unsuportedSystemsReport.insert(
                        id, UpdateStrings::getReportForUnsupportedOs(osInfo));
                    continue;

                case update::FindPackageResult::otherError:
                default:
                    NX_ERROR(typeid(UpdateContents),
                        "verifyUpdateManifest(%1) Update package is missing for %2",
                        contents.info.version, osInfo);
                    contents.missingUpdate.insert(id);
                    continue;
            }
        }

        if (serverVersion < targetVersion && server->isOnline())
        {
            contents.alreadyInstalled = false;
            contents.peersWithUpdate.insert(id);
            package->targets.insert(id);

            auto hasInternet = server->getServerFlags().testFlag(nx::vms::api::SF_HasPublicIP);
            if (!hasInternet)
                manualPackages.insert(package);
        }
    }

    for (auto package: manualPackages)
        contents.manualPackages.push_back(*package);

    if (contents.peersWithUpdate.empty())
        contents.invalidVersion.unite(serversWithNewerVersion);
    else
        contents.ignorePeers = serversWithNewerVersion;

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
                << contents.info.version << ") - we should install client package, but there is no such.";
            contents.error = nx::update::InformationError::missingPackageError;
            contents.missingUpdate.insert(clientData.clientId);
        }
        else
        {
            NX_WARNING(typeid(UpdateContents)) << "verifyUpdateManifest("
                << contents.info.version << ") - there is no client package, but applauncher has it. That's ok.";
        }
    }

    if (!contents.invalidVersion.empty() && !contents.alreadyInstalled)
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
            << contents.info.version
            << files.join(",");
    }

    return contents.isValidToInstall();
}

} // namespace nx::vms::client::desktop
