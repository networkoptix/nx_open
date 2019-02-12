#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QDir>
#include <QtCore/QSet>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/utils/software_version.h>
#include <nx/vms/api/data/system_information.h>

namespace nx::update {

struct Package
{
    QString component;
    QString arch;
    QString platform;
    QString variant;
    QString variantVersion;
    QString file;
    QString url;
    QString md5;
    qint64 size = 0;

    /** List of targets that should receive this package. */
    QList<QnUuid> targets;

    bool isValid() const { return !file.isEmpty(); }
};

#define Package_Fields (component)(arch)(platform)(variant)(variantVersion)(file)(url)(size)(md5)
QN_FUSION_DECLARE_FUNCTIONS(Package, (xml)(csv_record)(ubjson)(json)(eq))

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
    (description)(packages)(participants)(lastInstallationRequestTime)

QN_FUSION_DECLARE_FUNCTIONS(Information, (xml)(csv_record)(ubjson)(json)(eq))

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

    QnUuid serverId;
    Code code = Code::idle;
    QString message;
    double progress = 0.0;

    Status() = default;
    Status(
        const QnUuid& serverId,
        Code code,
        const QString& message = QString(),
        double progress = 0.0)
        :
        serverId(serverId),
        code(code),
        message(message),
        progress(progress)
    {}
};

#define UpdateStatus_Fields (serverId)(code)(progress)(message)

QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(Status::Code)
QN_FUSION_DECLARE_FUNCTIONS(Status::Code, (lexical))
QN_FUSION_DECLARE_FUNCTIONS(Status, (xml)(csv_record)(ubjson)(json))

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
 * Does comparison for package cache.
 */
struct SystemInformationComparator
{
    bool operator() (
        const nx::vms::api::SystemInformation& lhs,
        const nx::vms::api::SystemInformation& rhs) const
    {
        if (lhs.arch != rhs.arch)
            return lhs.arch < rhs.arch;

        if (lhs.platform != rhs.platform)
            return lhs.platform < rhs.platform;

        return lhs.modification < rhs.modification;
    }
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

    /** A set of servers without proper update file. */
    QSet<QnUuid> missingUpdate;
    /** A set of servers that can not accept update version. */
    QSet<QnUuid> invalidVersion;
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

    using SystemInformation = nx::vms::api::SystemInformation;

    using PackageCache =
        std::map<SystemInformation, QList<nx::update::Package>, SystemInformationComparator>;
    /**
     * Maps system information to a set of packages. We use this cache to find and check
     * if a specific OS variant is supported.
     */
    PackageCache serverPackageCache;
    PackageCache clientPackageCache;

    /** Information for the clent update. */
    nx::update::Package clientPackage;
    /** Flag shows that we need update package for client, but it is missing. */
    bool missingClientPackage = false;
    nx::update::Information info;
    nx::update::InformationError error = nx::update::InformationError::noError;
    /**
     * Packages for manual download. These packages should be downloaded by the client and
     * pushed to mediaservers without internet.
     */
    QList<Package> manualPackages;
    bool cloudIsCompatible = true;
    bool verified = false;
    /** We have already installed this version. Widget will show appropriate status.*/
    bool alreadyInstalled = false;

    nx::utils::SoftwareVersion getVersion() const;

    /** Check if we can apply this update. */
    bool isValidToInstall() const;

    /**
     * Check if this update info is completely empty.
     * Note: it differs a bit from isValid() check. We can remove some servers and make
     * update contents valid, but we can not do anything with an empty update.
     */
    bool isEmpty() const;

    /**
     * Compares this update info with 'other' and decides whether we should pick other one.
     * @return True if we need to pick 'other' update.
     */
    bool preferOtherUpdate(const UpdateContents& other) const;
};

} // namespace nx::update

Q_DECLARE_METATYPE(nx::update::Information);
Q_DECLARE_METATYPE(nx::update::UpdateContents);
