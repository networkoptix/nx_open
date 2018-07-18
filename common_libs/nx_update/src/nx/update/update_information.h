#pragma once

#include <QtCore>
#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace update {

const static QString kLatestVersion = "latest";

enum class NX_UPDATE_API Component
{
    server,
    client
};

QN_FUSION_DECLARE_FUNCTIONS(Component, (lexical), NX_UPDATE_API)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(Component)

struct NX_UPDATE_API Package
{
    Component component;
    QString arch;
    QString platform;
    QString variant;
    QString file;
    QString url;
    qint64 size;
    QByteArray md5;
};
#define Package_Fields (component)(arch)(platform)(variant)(file)(url)(size)(md5)
QN_FUSION_DECLARE_FUNCTIONS(Package, (xml)(csv_record)(ubjson)(json), NX_UPDATE_API)

struct NX_UPDATE_API Information
{
    QString version;
    QString cloudHost;
    QString eulaLink;
    int eulaVersion;
    QString releaseNotesUrl;
    QList<Package> packages;
    QList<QString> supportedOs;
    QList<QString> unsupportedOs;
};
#define Information_Fields (version)(cloudHost)(eulaLink)(eulaVersion)(releaseNotesUrl)(packages) \
    (supportedOs)(unsupportedOs)
QN_FUSION_DECLARE_FUNCTIONS(Information, (xml)(csv_record)(ubjson)(json), NX_UPDATE_API)

enum class NX_UPDATE_API InformationError
{
    noError,
    networkError,
    httpError,
    jsonError,
    incompatibleCloudHostError,
    notFoundError
};

QN_FUSION_DECLARE_FUNCTIONS(InformationError, (lexical), NX_UPDATE_API)
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(InformationError)

NX_UPDATE_API Information updateInformation(
    const QString& url,
    const QString& publicationKey = kLatestVersion,
    InformationError* error = nullptr);

NX_UPDATE_API Information updateInformation(
    const QString& zipFileName,
    InformationError* error = nullptr);

NX_UPDATE_API QString toString(InformationError error);

} // namespace update
} // namespace nx

