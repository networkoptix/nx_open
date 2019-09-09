#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QDir>
#include <QtCore/QSet>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/utils/software_version.h>
#include <nx/utils/os_info.h>

namespace nx::update {

struct Variant
{
    QString name;
    QString minimumVersion;
    QString maximumVersion;

    Variant(const QString& name = {}): name(name) {}
};
#define Variant_Fields (name)(minimumVersion)(maximumVersion)
QN_FUSION_DECLARE_FUNCTIONS(Variant, (ubjson)(json)(eq))

struct Package
{
    QString component;

    QString platform;
    QList<Variant> variants;

    QString file;
    QString url;
    QString md5;
    qint64 size = 0;

    // Internal fields for use by Clients and Servers (should not occur on update servers).

    /**
     * Local path to the package. This value is used locally by each peer.
     * Do not add it to fusion until it is really necessary.
     */
    QString localFile;

    /**
     * A set of targets that should receive this package.
     * This set is used by client to keep track of package uploads.
     */
    QSet<QnUuid> targets;

    bool isValid() const { return !file.isEmpty(); }
    bool isServer() const;
    bool isClient() const;

    bool isCompatibleTo(const utils::OsInfo& osInfo, bool ignoreVersion = false) const;
    bool isNewerThan(const QString& variant, const Package& other) const;
};

#define Package_Fields (component)(platform)(variants)(file)(url)(size)(md5)
QN_FUSION_DECLARE_FUNCTIONS(Package, (ubjson)(json)(eq))

struct Information
{
    QString version;
    QString cloudHost;
    QString eulaLink;
    int eulaVersion = 0;
    QString releaseNotesUrl;
    /** This is release notes for offline update package. */
    /** We need to be able to show this data without internet. */
    QString description;
    QString eula;
    QList<Package> packages;
    QList<QnUuid> participants;
    qint64 lastInstallationRequestTime = -1;

    /** Release date - in msecs since epoch. */
    qint64 releaseDateMs = 0;
    /** Maximum days for release delivery. */
    int releaseDeliveryDays = 0;

    bool isValid() const { return !version.isNull(); }
    bool isEmpty() const { return packages.isEmpty(); }
};

#define Information_Fields (version)(cloudHost)(eulaLink)(eulaVersion)(releaseNotesUrl) \
    (description)(packages)(participants)(lastInstallationRequestTime)(eula)(releaseDateMs) \
    (releaseDeliveryDays)

QN_FUSION_DECLARE_FUNCTIONS(Information, (ubjson)(json)(eq))

struct UpdateDeliveryInfo
{
    QString version;
    /** Release date - in msecs since epoch. */
    qint64 releaseDateMs = 0;
    /** Maximum days for release delivery. */
    int releaseDeliveryDays = 0;
};

#define UpdateDeliveryInfo_Fields (version)(releaseDateMs)(releaseDeliveryDays)
QN_FUSION_DECLARE_FUNCTIONS(UpdateDeliveryInfo, (ubjson)(json)(eq))

struct PackageInformation
{
    QString version;
    QString component;
    QString cloudHost;
    QString platform;
    QList<Variant> variants;
    QString installScript;
    qint64 freeSpaceRequired = 0;

    bool isValid() const { return !version.isNull(); }
    bool isServer() const;
    bool isClient() const;
    bool isCompatibleTo(const utils::OsInfo& osInfo) const;
};

#define PackageInformation_Fields (version)(component)(cloudHost)(platform)(variants) \
    (installScript)(freeSpaceRequired)

QN_FUSION_DECLARE_FUNCTIONS(PackageInformation, (json))

enum class InformationError
{
    noError,
    networkError,
    httpError,
    jsonError,
    brokenPackageError,
    missingPackageError,
    incompatibleVersion,
    incompatibleCloudHost,
    notFoundError,
    /** Error when client tried to contact mediaserver for update verification. */
    serverConnectionError,
    /**
     * Error when there's an attempt to parse packages.json for a version < 4.0 or an attempt to
     * parse update.json for a version >= 4.0.
     */
    incompatibleParser,
    noNewVersion,
};

QN_FUSION_DECLARE_FUNCTIONS(InformationError, (lexical))
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(InformationError)

QString toString(InformationError error);

struct Status
{
    Q_GADGET

public:
    enum class Code
    {
        idle,
        /** Server is downloading an update package. */
        downloading,
        /** Server is verifying an update package after downloading has finished successfully. */
        preparing,
        /** Update package has been downloaded and verified. */
        readyToInstall,
        latestUpdateInstalled,
        offline,
        error
    };
    Q_ENUM(Code)

    enum class ErrorCode
    {
        noError = 0,
        updatePackageNotFound,
        osVersionNotSupported,
        noFreeSpaceToDownload,
        noFreeSpaceToExtract,
        noFreeSpaceToInstall,
        downloadFailed,
        invalidUpdateContents,
        corruptedArchive,
        extractionError,
        internalDownloaderError,
        internalError,
        unknownError,
        applauncherError,
    };
    Q_ENUM(ErrorCode)

    QnUuid serverId;
    Code code = Code::idle;
    ErrorCode errorCode = ErrorCode::noError;
    QString message;
    int progress = 0;

    Status() = default;
    Status(
        const QnUuid& serverId,
        Code code,
        ErrorCode errorCode = ErrorCode::noError,
        int progress = 0)
        :
        serverId(serverId),
        code(code),
        errorCode(errorCode),
        progress(progress)
    {}

    bool suitableForRetrying() const { return code == Code::error || code == Code::idle; }

    friend inline uint qHash(nx::update::Status::ErrorCode key, uint seed)
    {
        return ::qHash(static_cast<uint>(key), seed);
    }
};

#define UpdateStatus_Fields (serverId)(code)(errorCode)(progress)(message)

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(Status::Code)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(Status::ErrorCode)
QN_FUSION_DECLARE_FUNCTIONS_FOR_TYPES((Status::Code)(Status::ErrorCode), (lexical))
QN_FUSION_DECLARE_FUNCTIONS(Status, (ubjson)(json))

/**
 * Source type for update information.
 */
enum class UpdateSourceType
{
    /** Got update info from the internet. */
    internet,
    /** Got update info from the internet for specific build. */
    internetSpecific,
    /** Got update info from offline update package. */
    file,
    /** Got update info from mediaserver swarm. */
    mediaservers,
};

/**
 * Wraps up update info and summary for its verification.
 * All update tools inside client work with this structure directly,
 * instead of nx::update::Information.
 */
struct UpdateContents
{
    UpdateSourceType sourceType = UpdateSourceType::internet;
    QString source;
    QString changeset;

    /** A set of servers without proper update file. */
    QSet<QnUuid> missingUpdate;
    /** A set of servers that can not accept update version. */
    QSet<QnUuid> invalidVersion;
    /** A set of peers to be ignored during this update. */
    QSet<QnUuid> ignorePeers;
    /** A set of peers with update packages verified. */
    QSet<QnUuid> peersWithUpdate;
    /**
     * Maps a server id, which OS is no longer supported to an error message.
     * The message is displayed to the user.
     */
    QMap<QnUuid, QString> unsuportedSystemsReport;
    /** Path to eula file. */
    QString eulaPath;
    /** A folder with offline update packages. */
    QDir storageDir;
    /** A list of files to be uploaded. */
    QStringList filesToUpload;
    /** Information for the clent update. */
    nx::update::Package clientPackage;

    nx::update::Information info;
    nx::update::InformationError error = nx::update::InformationError::noError;
    /**
     * Packages for manual download. These packages should be downloaded by the client and
     * pushed to mediaservers without internet.
     */
    QList<Package> manualPackages;

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

    nx::utils::SoftwareVersion getVersion() const;

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
    bool peerHasUpdate(const QnUuid& id) const;

    /**
     * Compares this update info with 'other' and decides whether we should pick other one.
     * @return True if we need to pick 'other' update.
     */
    bool preferOtherUpdate(const UpdateContents& other) const;
};

bool isPackageCompatibleTo(
    const QString& packagePlatform,
    const QList<Variant>& packageVariants,
    const utils::OsInfo& osInfo,
    bool ignoreVersion = false);

bool isPackageNewerForVariant(
    const QString& variant,
    const QList<Variant>& packageVariants,
    const QList<Variant>& otherPackageVariants);

} // namespace nx::update

Q_DECLARE_METATYPE(nx::update::Information);
Q_DECLARE_METATYPE(nx::update::UpdateContents);
Q_DECLARE_METATYPE(nx::update::UpdateDeliveryInfo);
