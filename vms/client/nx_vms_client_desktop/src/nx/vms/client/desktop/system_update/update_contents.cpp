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
    QList<nx::update::Package>& packages,
    const QString& component,
    const QString& variantVersion)
{
    nx::utils::SoftwareVersion version{variantVersion};
    for (auto& package: packages)
    {
        if (component != package.component)
            continue;

        if (package.variantVersion.isEmpty())
            return &package;

        if (nx::utils::SoftwareVersion(package.variantVersion) < version)
            return &package;
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
    const std::set<nx::utils::SoftwareVersion>& clientVersions)
{
    nx::update::Information& info = contents.info;
    if (contents.error != nx::update::InformationError::noError)
    {
        NX_ERROR(typeid(UpdateContents)) << "verifyUpdateManifest("
            << contents.info.version << ") has an error before verification:"
            << nx::update::toString(contents.error);
        return false;
    }
    // Hack to prevent double verification of update package.
    if (contents.verified)
        return contents.error != nx::update::InformationError::noError;

    if (contents.info.isEmpty())
    {
        contents.error = nx::update::InformationError::missingPackageError;
        return false;
    }

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
                    // This was breaking offline updates for client.
                    //pkg.file = uploadDestination + file.fileName();
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
            commonModule->moduleGUID(),
            commonModule->engineVersion(),
            systemInfo,
            contents.info,
            true, cloudUrl, boundToCloud, &contents.clientPackage, &errorMessage)
        != nx::update::FindPackageResult::ok)
    {
        NX_ERROR(typeid(UpdateContents))
            << "verifyUpdateManifest(" << contents.info.version
            << ") error while trying to find client package:" << errorMessage;
    }

    QSet<QnUuid> allServers;

    // We store here a set of packages that should be downloaded manually by the client.
    // We will convert it to a list of values later.
    QSet<nx::update::Package*> manualPackages;

    for (const auto& package: info.packages)
    {
        nx::vms::api::SystemInformation info;
        info.arch = package.arch;
        info.modification = package.variant;
        info.platform = package.platform;

        if (!contents.packageCache.count(info))
            contents.packageCache.emplace(info, QList<nx::update::Package>());
        auto& record = contents.packageCache[info];
        record.append(package);
    }

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

        auto it = contents.packageCache.find(serverInfo);
        if (it == contents.packageCache.end())
        {
            NX_ERROR(typeid(UpdateContents)) << "verifyUpdateManifest("
                << contents.info.version << ") server"
                << server->getId()
                << "arch" << serverInfo.arch
                << "platform" << serverInfo.platform
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
            package = findPackageForOsVariant(it->second, nx::update::kComponentServer, serverInfo.version);
        }

        nx::utils::SoftwareVersion serverVersion = server->getVersion();
        // Prohibiting updates to previous version.
        if (serverVersion > targetVersion)
        {
            NX_ERROR(typeid(UpdateContents)) << "verifyUpdateManifest("
                << contents.info.version << ") server"
                << server->getId()
                << "ver" << serverVersion.toString()
                << "is incompatible with this update";
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

    for (auto id: contents.unsuportedSystemsReport.keys())
        contents.missingUpdate.remove(id);

    if (!contents.missingUpdate.empty() || !contents.unsuportedSystemsReport.empty())
    {
        NX_WARNING(typeid(UpdateContents)) << "verifyUpdateManifest("
            << contents.info.version << ") - detected missing server packages.";
        contents.error = nx::update::InformationError::missingPackageError;
    }

    if (!contents.clientPackage.isValid())
    {
        QString clientVersion = nx::utils::AppInfo::applicationVersion();
        if (targetVersion > nx::utils::SoftwareVersion(clientVersion)
            && !clientVersions.count(targetVersion))
        {
            contents.missingClientPackage = true;
            NX_WARNING(typeid(UpdateContents)) << "verifyUpdateManifest("
                << contents.info.version << ") - we should install client package, but there are no such.";
            contents.error = nx::update::InformationError::missingPackageError;
        }
        else
        {
            NX_WARNING(typeid(UpdateContents)) << "verifyUpdateManifest("
                << contents.info.version << ") - there are no client package, but applauncher has it. That's ok.";
        }
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
