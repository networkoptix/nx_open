// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <set>
#include <map>

#include <QtCore/QCoreApplication>
#include <QtCore/QDir>
#include <QtCore/QStringList>
#include <QtCore/QCoreApplication>

#include <core/resource/resource_fwd.h>
#include <nx/vms/api/data/software_version.h>
#include <nx/vms/client/desktop/system_update/update_contents.h>
#include <nx/utils/os_info.h>
#include <nx/utils/uuid.h>

namespace nx::vms::client::desktop {

class SystemContext;

namespace update_verification {
// This property is used in unit tests to make update verification procedure think that a server is
// linked to cloud.
inline const char* kServerIsLinkedToCloudTestProperty = "__nx_server_is_linked_to_cloud";
} // namespace update_verification

struct UpdateStrings
{
    Q_DECLARE_TR_FUNCTIONS(UpdateStrings)

public:
    static QString getReportForUnsupportedOs(const nx::utils::OsInfo& info);
};

/**
 * Checks if cloud host is compatible with our system
 * @param commonModule
 * @param targetVersion - target version of system update.
 * @param cloudUrl - url to cloud from updateContents. We check it against our system.
 * @param peers - a set of peers to be checked.
 * @return true if specified cloudUrl is compatible with our system
 */
bool checkCloudHost(
    SystemContext* systemContext,
    nx::utils::SoftwareVersion targetVersion,
    QString cloudUrl,
    const QSet<QnUuid>& peers);

struct ClientVerificationData
{
    nx::utils::OsInfo osInfo;
    std::set<nx::utils::SoftwareVersion> installedVersions;
    /** Current client version, */
    nx::utils::SoftwareVersion currentVersion;
    /** Peer id for a client. Verification will ignore client if clientId is null. */
    QnUuid clientId;

    /** Fills in systemInfo and currentVersion. */
    void fillDefault();
};

struct VerificationOptions
{
    /** Client needs this update for compatibility mode. */
    bool compatibilityMode = false;
    /** Forcing client to download all necessary packages. */
    bool downloadAllPackages = true;
    /** It is needed for cloud compatibility check. */
    SystemContext* systemContext = nullptr;
};

/**
 * Run verification for update contents.
 * It checks whether there are all necessary packages and they are compatible with current system.
 * Result is stored at UpdateContents::missingUpdate, UpdateContents::invalidVersion and
 * UpdateContents::error fields of updateContents.
 * @param contents - update contents. Result is stored inside its fields.
 * @param servers - servers to be used for update verification.
 * @param clientData - contains additional client data necessary for verification.
 * @param options - verification options
 * @returns true if everything is ok. Detailed error can be found inside 'contents'.
 */
NX_VMS_CLIENT_DESKTOP_API bool verifyUpdateContents(
    client::desktop::UpdateContents& contents,
    const std::map<QnUuid, QnMediaServerResourcePtr>& servers,
    const ClientVerificationData& clientData, const VerificationOptions& options);

} // namespace nx::vms::client::desktop
