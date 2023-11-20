// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "update_verification.h"

#include <core/resource/media_server_resource.h>
#include <core/resource_management/resource_pool.h>
#include <network/system_helpers.h>
#include <nx/branding.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <nx/vms/client/desktop/application_context.h>
#include <nx/vms/client/desktop/other_servers/other_servers_manager.h>
#include <nx/vms/client/desktop/system_context.h>
#include <nx/vms/common/system_settings.h>
#include <nx/vms/discovery/manager.h>
#include <nx/vms/update/update_check.h>
#include <utils/common/synctime.h>

namespace {

const QString kFilePrefix = "file://";

} // namespace

namespace nx::vms::client::desktop {

QSet<QnUuid> getServersLinkedToCloud(SystemContext* systemContext, const QSet<QnUuid>& peers)
{
    QSet<QnUuid> result;

    const auto moduleManager = appContext()->moduleDiscoveryManager();
    if (!moduleManager)
        return result;

    for (const auto& id: peers)
    {
        if (!systemContext->otherServersManager()->containsServer(id))
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
    currentVersion = appContext()->version();
}

bool checkCloudHost(
    SystemContext* systemContext,
    nx::utils::SoftwareVersion targetVersion,
    QString cloudUrl,
    const QSet<QnUuid>& peers)
{
    NX_ASSERT(systemContext);
    /* Ignore cloud host for versions lower than 3.0. */
    static const nx::utils::SoftwareVersion kCloudRequiredVersion(3, 0);

    if (targetVersion < kCloudRequiredVersion)
        return true;

    /* Update is allowed if either target version has the same cloud host or
       there are no servers linked to the cloud in the system. */
    if (cloudUrl == nx::network::SocketGlobals::cloud().cloudHost())
        return true;

    const bool isBoundToCloud = !systemContext->globalSettings()->cloudSystemId().isEmpty();
    if (isBoundToCloud)
        return false;

    const auto serversLinkedToCloud = getServersLinkedToCloud(systemContext, peers);
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
        if (ver.major == 6)
        {
            switch (ver.minor)
            {
                case 0: version = "Vista"; break;
                case 1: version = "7"; break;
                case 2: version = "8"; break;
                case 3: version = "8.1"; break;
                default: break;
            }
        }
        else if (ver.major == 10)
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
    client::desktop::UpdateContents& contents,
    const std::map<QnUuid, QnMediaServerResourcePtr>& activeServers,
    const ClientVerificationData& clientData,
    const VerificationOptions& options)
{
    nx::utils::log::Tag logTag(nx::format("verifyUpdateContents(%1)", contents.info.version));

    if (contents.error != nx::vms::common::update::InformationError::noError)
    {
        NX_ERROR(logTag, "has an error before verification: %1", contents.error);
        return false;
    }

    if (contents.info.isEmpty())
    {
        contents.error = nx::vms::common::update::InformationError::missingPackageError;
        return false;
    }

    if (clientData.clientId.isNull())
    {
        NX_DEBUG(logTag, "verifying updates for %1 mediaservers", activeServers.size());
    }
    else
    {
        NX_DEBUG(logTag, "verifying updates for %1 mediaservers and client",
            activeServers.size());
    }

    nx::utils::SoftwareVersion targetVersion(contents.info.version);

    if (contents.info.version.isNull() || targetVersion.isNull())
    {
        NX_ERROR(typeid(UpdateContents),
            "verifyUpdateContents(...) - missing or invalid version.");
        contents.error = nx::vms::common::update::InformationError::jsonError;
        return false;
    }

    if (clientData.currentVersion == targetVersion)
    {
        const auto releaseDate = contents.info.releaseDate.count();
        if (releaseDate > 0 && qnSyncTime->currentMSecsSinceEpoch() >= releaseDate)
        {
            contents.error = nx::vms::common::update::InformationError::noNewVersion;
            return false;
        }
    }

    contents.invalidVersion.clear();
    contents.missingUpdate.clear();
    contents.unsuportedSystemsReport.clear();
    contents.peersWithUpdate.clear();
    contents.manualPackages.clear();
    contents.noServerWithInternet = true;

    // Check if some packages from manifest do not exist.
    if (contents.sourceType == client::desktop::UpdateSourceType::file
        && !contents.packagesGenerated)
    {
        contents.filesToUpload.clear();
        QString uploadDestination = QString("updates/%1/").arg(targetVersion.build);
        QList<nx::vms::update::Package> checked;
        for (auto& pkg: contents.info.packages)
        {
            if (contents.sourceType == client::desktop::UpdateSourceType::file)
            {
                QFileInfo file(contents.storageDir.filePath(pkg.file));
                if (file.exists())
                {
                    // In 4.0 we should upload only server components. There is no way to make
                    // sure server keeps client packages.
                    if (pkg.component == update::Component::server)
                        contents.filesToUpload.push_back(pkg.file);

                    // This filename will be used by uploader.
                    pkg.file = uploadDestination + file.fileName();
                    contents.packageProperties[pkg.file].localFile = file.absoluteFilePath();
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
        contents.eulaPath = contents.info.eulaLink;
    }

    // We will try to set it to false when we check servers and clients.
    contents.alreadyInstalled = true;
    bool clientInstalledThis = clientData.installedVersions.count(targetVersion) != 0;

    if (!clientData.clientId.isNull()
        && (clientData.currentVersion < targetVersion
            || (clientData.currentVersion > targetVersion && options.compatibilityMode))
        && !clientInstalledThis)
    {
        contents.alreadyInstalled = false;
        contents.needClientUpdate = true;

        const update::PublicationInfo::FindPackageResult searchResult = contents.info.findPackage(
            update::Component::client, clientData.osInfo, branding::customClientVariant());
        if (const auto& package = std::get_if<update::Package>(&searchResult))
        {
            contents.clientPackage = *package;
            contents.peersWithUpdate.insert(clientData.clientId);
        }
        else
        {
            const auto error = std::get<update::FindPackageError>(searchResult);
            switch (error)
            {
                case update::FindPackageError::osVersionNotSupported:
                {
                    NX_ERROR(logTag, "OS version is not supported: %1", clientData.osInfo);
                    contents.unsuportedSystemsReport.insert(
                        clientData.clientId,
                        UpdateStrings::getReportForUnsupportedOs(clientData.osInfo));
                }
                case update::FindPackageError::notFound:
                {
                    NX_ERROR(logTag, "Update package is missing for %1", clientData.osInfo);
                    contents.missingUpdate.insert(clientData.clientId);
                }
            }
        }
    }
    else
    {
        contents.needClientUpdate = false;
    }

    QHash<nx::utils::OsInfo, update::Package> serverPackages;

    QSet<QnUuid> allServers;

    // We store here a set of packages that should be downloaded manually by the client.
    // We will convert it to a list of values later.
    QSet<nx::vms::update::Package> manualPackages;

    QSet<QnUuid> serversWithNewerVersion;
    QSet<QnUuid> serversWithExactVersion;

    // TODO: Should reverse this verification: get all platform configurations and find packages for them.
    // Checking if all servers have update packages.
    for (auto& [id, server]: activeServers)
    {
        allServers.insert(id);

        const auto& osInfo = server->getOsInfo();
        if (!osInfo.isValid())
        {
            NX_WARNING(logTag, "server %1 has invalid osInfo", id);
        }

        const bool hasInternet = server->hasInternetAccess();
        contents.noServerWithInternet |= hasInternet;

        const nx::utils::SoftwareVersion serverVersion = server->getVersion();

        if (serverVersion > targetVersion)
        {
            NX_WARNING(logTag, "Server %1 has a newer version %2", id, serverVersion);
            serversWithNewerVersion.insert(id);
            continue;
        }
        else if (serverVersion == targetVersion)
        {
            NX_DEBUG(logTag, "Server %1 is already at this version", id);
            serversWithExactVersion.insert(id);
        }

        update::Package package = serverPackages[osInfo];
        if (package.file.isEmpty())
        {
            const update::PublicationInfo::FindPackageResult result =
                contents.info.findPackage(update::Component::server, osInfo);

            if (const auto& foundPackage = std::get_if<update::Package>(&result))
            {
                package = *foundPackage;
                serverPackages[osInfo] = package;
            }
            else
            {
                const auto error = std::get<update::FindPackageError>(result);
                switch (error)
                {
                    case update::FindPackageError::osVersionNotSupported:
                        NX_ERROR(logTag, "OS version is not supported: %1", osInfo);
                        contents.unsuportedSystemsReport.insert(
                            id, UpdateStrings::getReportForUnsupportedOs(osInfo));
                        continue;

                    case update::FindPackageError::notFound:
                    default:
                        NX_ERROR(logTag, "Update package is missing for %1", osInfo);
                        contents.missingUpdate.insert(id);
                        continue;
                }
            }
        }

        // Prohibiting updates to previous version.
        if (serverVersion < targetVersion)
        {
            contents.alreadyInstalled = false;
            contents.peersWithUpdate.insert(id);
            contents.packageProperties[package.file].targets.insert(id);
            if (!hasInternet || options.downloadAllPackages)
                manualPackages.insert(package);
        }
    }

    for (auto package: manualPackages)
        contents.manualPackages.insert(package);

    if (contents.peersWithUpdate.empty())
    {
        contents.invalidVersion.unite(serversWithNewerVersion);
    }
    else
    {
        // Since we have some peers with valid update, we can ignore problems for servers with
        // exact version.
        contents.invalidVersion -= serversWithExactVersion;
        contents.missingUpdate -= serversWithExactVersion;
        contents.ignorePeers = serversWithNewerVersion;
    }

    if (options.systemContext)
    {
        contents.cloudIsCompatible = checkCloudHost(
            options.systemContext, targetVersion, contents.info.cloudHost, allServers);
    }

    for (auto id: contents.unsuportedSystemsReport.keys())
        contents.missingUpdate.remove(id);

    if (!contents.missingUpdate.empty() || !contents.unsuportedSystemsReport.empty())
    {
        NX_WARNING(logTag, "detected missing packages.");
        contents.error = common::update::InformationError::missingPackageError;
    }

    if (!clientData.clientId.isNull()
        && (contents.missingUpdate.contains(clientData.clientId) || !contents.clientPackage))
    {
        if (targetVersion > clientData.currentVersion
            && !clientData.installedVersions.count(targetVersion))
        {
            NX_WARNING(logTag, "we should install client package, but there is no such.");
            if (!contents.unsuportedSystemsReport.contains(clientData.clientId))
            {
                contents.error = common::update::InformationError::missingPackageError;
                contents.missingUpdate.insert(clientData.clientId);
            }
        }
        else
        {
            NX_WARNING(logTag, "there is no client package, but applauncher has it. That's ok.");
        }
    }

    // According to VMS-14494 we should allow update if some servers have more recent version
    // than update, but some servers can be updated to it.
    if (!contents.invalidVersion.empty()
        && (!contents.alreadyInstalled || contents.peersWithUpdate.empty()))
    {
        NX_WARNING(logTag, "detected incompatible version error.");
        contents.error = common::update::InformationError::incompatibleVersion;
    }

    // Update package has no packages at all.
    if (contents.info.packages.empty() && !activeServers.empty())
    {
        NX_WARNING(logTag, "this update is completely empty.");
        contents.error = common::update::InformationError::missingPackageError;
    }

    if (!contents.cloudIsCompatible)
    {
        NX_WARNING(logTag, "detected incompatible cloud.");
        contents.error = common::update::InformationError::incompatibleCloudHost;
    }

    if (!contents.manualPackages.empty())
    {
        QStringList files;
        for (const auto& pkg: contents.manualPackages)
            files.append(pkg.file);

        NX_WARNING(logTag, "detected some servers can not download update packages: %1",
            files.join(", "));
    }

    return contents.isValidToInstall();
}

} // namespace nx::vms::client::desktop
