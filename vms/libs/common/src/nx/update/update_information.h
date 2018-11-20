#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <QtCore/QDir>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>
#include <nx/utils/software_version.h>

namespace nx {
namespace update {

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

    bool isValid() const { return !file.isEmpty(); }
};

#define Package_Fields (component)(arch)(platform)(variant)(variantVersion)(file)(url)(size)(md5)
QN_FUSION_DECLARE_FUNCTIONS(Package, (xml)(csv_record)(ubjson)(json))

struct Information
{
    QString version;
    QString cloudHost;
    QString eulaLink;
    int eulaVersion = 0;
    QString releaseNotesUrl;
    // This is release notes for offline update package.
    // We need to be able to show this data without internet.
    QString description;
    QList<Package> packages;

    // Release date - in msecs since epoch.
    qint64 releaseDateMs = 0;
    // Maximum days for release delivery.
    int releaseDeliveryDays = 0;

    bool isValid() const { return !version.isNull(); }
    bool isEmpty() const { return packages.isEmpty(); }
};

#define Information_Fields (version)(cloudHost)(eulaLink)(eulaVersion)(releaseNotesUrl)(description)(packages)
QN_FUSION_DECLARE_FUNCTIONS(Information, (xml)(csv_record)(ubjson)(json))

enum class InformationError
{
    noError,
    networkError,
    httpError,
    jsonError,
    brokenPackageError,
    missingPackageError,
    incompatibleCloudHostError,
    incompatibleVersion,
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
        // Server is downloading an update package.
        downloading,
        // Server is verifying an update package after downloading has finished successfully.
        preparing,
        // Update package has been downloaded and verified.
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
 * All update tools inside client work with this structure directly,
 * instead of nx::update::Information
 */
struct UpdateContents
{
    UpdateSourceType sourceType = UpdateSourceType::internet;
    QString source;

    // A set of servers without proper update file.
    QSet<QnUuid> missingUpdate;
    // A set of servers that can not accept update version.
    QSet<QnUuid> invalidVersion;

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

} // namespace update
} // namespace nx

Q_DECLARE_METATYPE(nx::update::Information);
Q_DECLARE_METATYPE(nx::update::UpdateContents);
