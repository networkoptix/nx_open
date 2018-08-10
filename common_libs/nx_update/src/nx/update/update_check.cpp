#include "update_check.h"
#include <QJsonObject>
#include <nx/fusion/model_functions.h>
#include <nx/network/http/http_async_client.h>
#include <nx/utils/std/future.h>
#include <nx/utils/log/log.h>
#include <utils/common/app_info.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/vms/api/data/software_version.h>
#include <nx/utils/raii_guard.h>

namespace nx {
namespace update {

struct AlternativeServerData
{
    QString url;
    QString name;
};

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    AlternativeServerData, (json),
    (url)(name))

struct CustomizationInfo
{
    QString current_release;
    QString updates_prefix;
    QString release_notes;
    QString description;
    QMap<QString, nx::vms::api::SoftwareVersion> releases;
};

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    CustomizationInfo,
    (json),
    (current_release)(updates_prefix)(release_notes)(description)(releases))

struct FileData
{
    QString file;
    QString md5;
    qint64 size;
};

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    FileData, (json),
    (file)(md5)(size))


namespace {

static InformationError makeHttpRequest(
    nx::network::http::AsyncClient* httpClient,
    const QString& url)
{
    nx::utils::promise<void> readyPomise;
    nx::utils::future<void> readyFuture = readyPomise.get_future();
    InformationError error = InformationError::noError;

    httpClient->doGet(
        url,
        [&]()
        {
            if (httpClient->state() != nx::network::http::AsyncClient::State::sDone)
            {
                error = InformationError::networkError;
                NX_WARNING(
                    typeid(Information),
                    lm("Failed to get update info from the internet, url: %1").args(url));
                readyPomise.set_value();
                return;
            }

            int statusCode = httpClient->response()->statusLine.statusCode;
            if (statusCode != network::http::StatusCode::ok)
            {
                error = InformationError::httpError;
                NX_WARNING(
                    typeid(Information),
                    lm("Http client http error: %1, url: %2").args(statusCode, url));
            }

            readyPomise.set_value();
        });

     readyFuture.wait();
     return error;
}

const static QString kAlternativesServersKey = "__info";

static InformationError findCustomizationInfo(
    const QJsonObject topLevelObject,
    CustomizationInfo* customizationInfo,
    Information* result)
{
    for (auto it = topLevelObject.constBegin(); it != topLevelObject.constEnd(); ++it)
    {
        if (it.key() == kAlternativesServersKey)
            continue;

        if (!QJson::deserialize(topLevelObject, it.key(), customizationInfo))
        {
            NX_ERROR(
                typeid(Information),
                lm("Customization json parsing failed: %1").args(it.key()));
            return InformationError::jsonError;
        }

        if (QnAppInfo::customizationName() == it.key())
        {
            result->releaseNotesUrl = customizationInfo->release_notes;
            return InformationError::noError;
        }
    }

    NX_ERROR(
        typeid(Information),
        lm("Customization %1 not found").args(QnAppInfo::customizationName()));

    return InformationError::jsonError;
}

static InformationError parsePackages(
    const QJsonObject topLevelObject,
    const QString& baseUpdateUrl,
    const QString& publicationKey,
    Information* result)
{
    QList<Package> packages;
    if (!QJson::deserialize(topLevelObject, "packages", &packages))
        return InformationError::jsonError;

    for (const auto& p: packages)
    {
        Package newPackage = p;
        newPackage.file = "updates/" + publicationKey + '/' + p.file;
        newPackage.url = baseUpdateUrl + "/" + p.file;
        result->packages.append(newPackage);
    }

    return InformationError::noError;
}

static InformationError parseAndExtractInformation(
    const QByteArray& data,
    const QString& baseUpdateUrl,
    const QString& publicationKey,
    Information* result)
{
    QJsonParseError parseError;
    auto topLevelObject = QJsonDocument::fromJson(data, &parseError).object();
    if (parseError.error != QJsonParseError::ParseError::NoError || topLevelObject.isEmpty())
        return InformationError::jsonError;

    if (!QJson::deserialize(topLevelObject, "version", &result->version)
        || !QJson::deserialize(topLevelObject, "cloudHost", &result->cloudHost)
        || !QJson::deserialize(topLevelObject, "eulaVersion", &result->eulaVersion)
        || !QJson::deserialize(topLevelObject, "eulaLink", &result->eulaLink))
    {
        return InformationError::jsonError;
    }

    return parsePackages(topLevelObject, baseUpdateUrl, publicationKey, result);
}

static InformationError fillUpdateInformation(
    nx::network::http::AsyncClient* httpClient,
    const QString& publicationKey,
    Information* result,
    QList<AlternativeServerData>* alternativeServers)
{
    QJsonParseError parseError;
    auto data = httpClient->fetchMessageBodyBuffer();
    auto topLevelObject = QJsonDocument::fromJson(data, &parseError).object();

    if (parseError.error != QJsonParseError::ParseError::NoError || topLevelObject.isEmpty())
        return InformationError::jsonError;

    if (!QJson::deserialize(topLevelObject, kAlternativesServersKey, alternativeServers))
        return InformationError::jsonError;

    CustomizationInfo customizationInfo;
    InformationError error = findCustomizationInfo(topLevelObject, &customizationInfo, result);
    if (error != InformationError::noError)
        return error;

    auto baseUpdateUrl = customizationInfo.updates_prefix + "/" + publicationKey;
    error = makeHttpRequest(httpClient, baseUpdateUrl + "/packages.json");

    if (error != InformationError::noError)
        return error;

    return parseAndExtractInformation(
        httpClient->fetchMessageBodyBuffer(),
        baseUpdateUrl,
        publicationKey,
        result);
}

Information updateInformationImpl(
    const QString& url,
    const QString& publicationKey,
    InformationError* error,
    bool checkAlternativeServers)
{
    nx::network::http::AsyncClient httpClient;
    Information result;
    InformationError localError = makeHttpRequest(&httpClient, url);
    auto onStop = QnRaiiGuard::createDestructible([&httpClient](){ httpClient.pleaseStopSync(); });

    if (localError != InformationError::noError)
    {
        if (error)
            *error = localError;
        return result;
    }

    QList<AlternativeServerData> alternativeServers;
    localError = fillUpdateInformation(&httpClient, publicationKey, &result, &alternativeServers);
    if (localError != InformationError::noError && checkAlternativeServers)
    {
        for (const auto& alternativeServer: alternativeServers)
        {
            result = updateInformationImpl(
                alternativeServer.url,
                publicationKey,
                &localError,
                /*checkAlternativeServers*/ false);

            if (localError == InformationError::noError)
                break;
        }
    }

    if (error)
        *error = localError;

    return result;
}

} // namespace

Information updateInformation(
    const QString& url,
    const QString& publicationKey,
    InformationError* error)
{
    return updateInformationImpl(url, publicationKey, error, /*checkAlternativeServers*/ true);
}

Information updateInformation(const QString& /*zipFileName*/, InformationError* /*error*/)
{
    return Information();
}

bool findPackage(
    const vms::api::SystemInformation& systemInformation,
    const nx::update::Information& updateInformation,
    bool isClient,
    const QString& cloudHost,
    bool boundToCloud,
    nx::update::Package* outPackage)
{
    if (updateInformation.cloudHost != cloudHost && boundToCloud)
        return false;

    for (const auto& package : updateInformation.packages)
    {
        if (isClient != (package.component == update::kComponentClient))
            continue;

        nx::utils::SoftwareVersion selfOsVariant(systemInformation.version);
        nx::utils::SoftwareVersion packageOsVariant(package.variantVersion);

        if (package.arch == systemInformation.arch
            && package.platform == systemInformation.platform
            && package.variant == systemInformation.modification
            && packageOsVariant <= selfOsVariant)
        {
            *outPackage = package;
            return true;
        }
    }

    return false;
}

bool findPackage(
    const vms::api::SystemInformation& systemInformation,
    const QByteArray& serializedUpdateInformation,
    bool isClient,
    const QString& cloudHost,
    bool boundToCloud,
    nx::update::Package* outPackage)
{
    update::Information updateInformation;
    if (!QJson::deserialize(serializedUpdateInformation, &updateInformation))
        return false;

    return findPackage(systemInformation, updateInformation, isClient, cloudHost, boundToCloud,
        outPackage);
}

} // namespace update
} // namespace nx

