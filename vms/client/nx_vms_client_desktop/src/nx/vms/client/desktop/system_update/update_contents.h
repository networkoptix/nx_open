#pragma once

#include <QtCore/QDir>
#include <QtCore/QStringList>
#include <future>

#include <core/resource/resource_fwd.h>
#include <nx/update/update_information.h>
#include <nx/vms/api/data/software_version.h>

class QnCommonModule;

namespace nx::vms::client::desktop {

enum class UpdateSourceType
{
    // Got update info from the internet.
    internet,
    // Got update info from the internet for specific build.
    internetSpecific,
    // Got update info from offline update package.
    file,
    // Got update info from mediaserver swarm.
    mediaservers,
};

/**
 * Wraps up update info and summary for its verification.
 */
struct UpdateContents
{
    UpdateSourceType sourceType = UpdateSourceType::internet;
    QString source;

    // A set of servers without proper update file.
    QSet<QnMediaServerResourcePtr> missingUpdate;
    // A set of servers that can not accept update version.
    QSet<QnMediaServerResourcePtr> invalidVersion;

    QString eulaPath;
    // A folder with offline update packages.
    QDir storageDir;
    // A list of files to be uploaded.
    QStringList filesToUpload;
    // Information for the clent update.
    nx::update::Package clientPackage;
    nx::update::Information info;
    nx::update::InformationError error = nx::update::InformationError::noError;
    bool cloudIsCompatible = true;
    bool verified = false;

    nx::utils::SoftwareVersion getVersion() const;
    // Check if we can apply this update.
    bool isValid() const;
};

/**
 * Search update contents for the client package, suitable for current architecture/platform.
 * @param updateInfo - update information. It can be obtained from the internet, offline update
 * or a set of mediaservers
 * @return package data
 */
nx::update::Package findClientPackage(const nx::update::Information& updateInfo);

/**
 * Tries to find update package for the specified server.
 * @param server
 * @param updateInfo
 * @return pointer to package found, or nullptr.
 */
const nx::update::Package* findPackageForServer(
    QnMediaServerResourcePtr server, const nx::update::Information& updateInfo);

/**
 * Checks if cloud host is compatible with our system
 * @param commonModule
 * @param targetVersion - target version of system update.
 * @param cloudUrl - url to cloud from updateContents. We check it against our system.
 * @param peers - a set of peers to be checked.
 * @return true if specified cloudUrl is compatible with our system
 */
bool checkCloudHost(
    QnCommonModule* commonModule,
    nx::utils::SoftwareVersion targetVersion,
    QString cloudUrl,
    const QSet<QnUuid>& peers);

/**
 * Run verification for update contents.
 * It checks whether there are all necessary packages and they are compatible with current system.
 * Result is stored at UpdateContents::missingUpdate, UpdateContents::invalidVersion and
 * UpdateContents::error fields of updateContents.
 * @param commonModule - everybody needs commonModule
 * @param contents - update contents. Result is stored inside its fields
 * @param servers - servers to be used for update verification
 * @returns true if everything is ok.
 */
bool verifyUpdateContents(QnCommonModule* commonModule, UpdateContents& contents,
    std::map<QnUuid, QnMediaServerResourcePtr> servers);

std::future<UpdateContents> checkLatestUpdate();
std::future<UpdateContents> checkSpecificChangeset(QString build);

} // namespace nx::vms::client::desktop
