#pragma once

#include <QObject>
#include <QString>
#include <nx/fusion/model_functions_fwd.h>
#include <nx/utils/uuid.h>

namespace nx {
namespace update {

namespace detail {
struct BasePackage
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

#define BasePackage_Fields (component)(arch)(platform)(variant)(variantVersion)(file)(size)(md5)
QN_FUSION_DECLARE_FUNCTIONS(BasePackage, (xml)(csv_record)(ubjson)(json))

} // namespace detail

struct Package: detail::BasePackage
{
    Package() = default;
    Package(const Package&) = default;
    Package(const BasePackage& basePackage): BasePackage(basePackage) {}
    QString url;
};

#define Package_Fields BasePackage_Fields (url)
QN_FUSION_DECLARE_FUNCTIONS(Package, (xml)(csv_record)(ubjson)(json))

struct Information
{
    QString version;
    QString cloudHost;
    QString eulaLink;
    int eulaVersion = 0;
    QString releaseNotesUrl;
    QList<Package> packages;

    bool isValid() const { return !version.isNull(); }
};
#define Information_Fields (version)(cloudHost)(eulaLink)(eulaVersion)(releaseNotesUrl)(packages)
QN_FUSION_DECLARE_FUNCTIONS(Information, (xml)(csv_record)(ubjson)(json))

enum class InformationError
{
    noError,
    networkError,
    httpError,
    jsonError,
    incompatibleCloudHostError,
    notFoundError
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
        downloading,
        readyToInstall,
        preparing,
        offline,
        installing,
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

