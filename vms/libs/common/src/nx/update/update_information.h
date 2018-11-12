#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>

#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

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
    // This is EULA text for offline update package.
    // We need to be able to show this data without internet.
    QString eulaContents;
    int eulaVersion = 0;
    QString releaseNotesUrl;
    // This is release notes for offline update package.
    // We need to be able to show this data without internet.
    QString releaseNotesContents;
    QList<Package> packages;

    bool isValid() const { return !version.isNull(); }
    bool isEmpty() const { return packages.isEmpty(); }
};

#define Information_Fields (version)(cloudHost)(eulaLink)(eulaContents)(eulaVersion)(releaseNotesUrl)(releaseNotesContents)(packages)
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

} // namespace update
} // namespace nx

