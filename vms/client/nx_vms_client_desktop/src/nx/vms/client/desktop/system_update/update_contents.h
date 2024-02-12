// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#pragma once

#include <chrono>

#include <QtCore/QDir>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QSet>
#include <QtCore/QString>

#include <nx/reflect/instrument.h>
#include <nx/vms/common/update/update_information.h>

namespace nx::vms::client::desktop {

struct UpdateDeliveryInfo
{
    nx::utils::SoftwareVersion version;
    /** Release date - in msecs since epoch. */
    std::chrono::milliseconds releaseDateMs{0};
    /** Maximum days for release delivery. */
    int releaseDeliveryDays = 0;

    bool operator==(const UpdateDeliveryInfo& other) const = default;
};
#define UpdateDeliveryInfo_Fields (version)(releaseDateMs)(releaseDeliveryDays)
NX_REFLECTION_INSTRUMENT(UpdateDeliveryInfo, UpdateDeliveryInfo_Fields)

/**
 * Source type for update information.
 */
enum class UpdateSourceType
{
    /** Got update info from the Internet. */
    internet,
    /** Got update info from the Internet for specific build. */
    internetSpecific,
    /** Got update info from offline update package. */
    file,
    /** Got update info from mediaserver swarm. */
    mediaservers,
};

/**
 * Wraps up update info and summary for its verification.
 * All update tools inside client work with this structure directly,
 * instead of nx::vms::common::update::Information.
 */
struct NX_VMS_CLIENT_DESKTOP_API UpdateContents
{
    UpdateSourceType sourceType = UpdateSourceType::internet;
    QString source;

    /** A set of servers without proper update file. */
    QSet<nx::Uuid> missingUpdate;
    /** A set of servers that can not accept update version. */
    QSet<nx::Uuid> invalidVersion;
    /** A set of peers to be ignored during this update. */
    QSet<nx::Uuid> ignorePeers;
    /** A set of peers with update packages verified. */
    QSet<nx::Uuid> peersWithUpdate;
    /**
     * Maps a server id, which OS is no longer supported to an error message.
     * The message is displayed to the user.
     */
    QMap<nx::Uuid, QString> unsuportedSystemsReport;
    /** Path to eula file. */
    QString eulaPath;
    /** A folder with offline update packages. */
    QDir storageDir;
    /** A list of files to be uploaded. */
    QStringList filesToUpload;
    /** Information for the clent update. */
    std::optional<update::Package> clientPackage;

    common::update::Information info;
    common::update::InformationError error = common::update::InformationError::noError;
    /**
     * Packages for manual download. These packages should be downloaded by the client and
     * pushed to mediaservers without internet.
     */
    QSet<update::Package> manualPackages;

    struct PackageProperties
    {
        /**
         * Local path to the package. This value is used locally by each peer.
         * Do not add it to fusion until it is really necessary.
         */
        QString localFile;

        /**
         * A set of targets that should receive this package.
         * This set is used by client to keep track of package uploads.
         */
        QSet<nx::Uuid> targets;
    };
    QHash<QString, PackageProperties> packageProperties;

    bool cloudIsCompatible = true;

    bool packagesGenerated = false;
    /** We have already installed this version. Widget will show appropriate status.*/
    bool alreadyInstalled = false;

    bool needClientUpdate = false;

    bool noServerWithInternet = true;

    /** Resets data from verification. */
    void resetVerification();

    /** Estimated space to keep manual packages. */
    uint64_t getClientSpaceRequirements(bool withClient) const;

    UpdateDeliveryInfo getUpdateDeliveryInfo() const;

    /** Check if we can apply this update. */
    bool isValidToInstall() const;

    /**
     * Check if this update info is completely empty.
     * Note: it differs a bit from isValid() check. We can remove some servers and make
     * update contents valid, but we can not do anything with an empty update.
     */
    bool isEmpty() const;

    /** Checks if peer has update package. */
    bool peerHasUpdate(const nx::Uuid& id) const;

    /**
     * Compares this update info with 'other' and decides whether we should pick other one.
     * @return True if we need to pick 'other' update.
     */
    bool preferOtherUpdate(const UpdateContents& other) const;
};

} // namespace nx::vms::client::desktop
