#pragma once

#include <QtCore>
#include <nx/fusion/model_functions_fwd.h>

namespace nx {
namespace update {

const static QString kLatestVersion = "0.0.0.0";

enum class Component
{
    server,
    client
};

QN_FUSION_DECLARE_FUNCTIONS(Component, (lexical))
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(Component)

struct Package
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
QN_FUSION_DECLARE_FUNCTIONS(Package, (xml)(csv_record)(ubjson)(json))

struct Information
{
    QString version;
    QString cloudHost;
    QString eulaLink;
    int eulaVersion;
    QString releaseNotesUrl;
    QList<Package> packages;
};

#define Information_Fields (version)(cloudHost)(eulaLink)(eulaVersion)(releaseNotesUrl)(packages)
QN_FUSION_DECLARE_FUNCTIONS(Information, (xml)(csv_record)(ubjson)(json))

enum class InformationError
{
    noError,
    networkError,
    httpError,
    jsonError,
    incompatibleCloudHost
};

QN_FUSION_DECLARE_FUNCTIONS(InformationError, (lexical))
QN_ENABLE_ENUM_NUMERIC_SERIALIZATION(InformationError)

Information updateInformation(
    const QString& url,
    const QString& publicationKey = kLatestVersion,
    InformationError* error = nullptr);

Information updateInformation(
    const QString& zipFileName,
    InformationError* error = nullptr);

} // namespace update
} // namespace nx

