#pragma once

#include <future>
#include <set>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QStringList>

#include <core/resource/resource_fwd.h>
#include <nx/update/update_information.h>
#include <nx/vms/api/data/software_version.h>
#include <nx/utils/uuid.h>

class QnCommonModule;

namespace nx::vms::client::desktop {

struct UpdateStrings
{
    Q_DECLARE_TR_FUNCTIONS(UpdateStrings);

public:
    static QString getReportForUnsupportedServer(const nx::vms::api::SystemInformation& info);
};

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
 * @param commonModule - everybody needs commonModule.
 * @param contents - update contents. Result is stored inside its fields.
 * @param servers - servers to be used for update verification.
 * @param clientVersions - a set of installed client versions.
 * @param clientId - peer id for a client. Verification will ignore client if clientId is null.
 * @returns true if everything is ok.
 */
bool verifyUpdateContents(
    QnCommonModule* commonModule,
    nx::update::UpdateContents& contents,
    std::map<QnUuid, QnMediaServerResourcePtr> servers,
    const std::set<nx::utils::SoftwareVersion>& clientVersions = {}, QnUuid clientId = QnUuid());

} // namespace nx::vms::client::desktop
