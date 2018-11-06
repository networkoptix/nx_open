#pragma once

#include <QtCore/QDir>
#include <QtCore/QStringList>

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

    nx::utils::SoftwareVersion getVersion() const;
    // Check if we can apply this update.
    bool isValid() const;
};

nx::update::Package findClientPackage(const nx::update::Information& updateInfo);

bool checkCloudHost(
    QnCommonModule* commonModule,
    nx::utils::SoftwareVersion targetVersion,
    QString cloudUrl,
    const QSet<QnUuid>& peers);

} // namespace nx::vms::client::desktop
