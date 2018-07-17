#include "update_information.h"
#include <QJsonObject>
#include <nx/fusion/model_functions.h>
#include <nx/network/http/http_async_client.h>
#include <nx/utils/std/future.h>
#include <nx/utils/log/log.h>
#include <nx/network/socket_global.h>
#include <utils/common/app_info.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/vms/api/data/software_version.h>

namespace nx {
namespace update {

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(
    nx::update, Component,
    (nx::update::Component::client, "client")
    (nx::update::Component::server, "server"))

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(
    nx::update, InformationError,
    (nx::update::InformationError::noError, "no error")
    (nx::update::InformationError::networkError, "network error")
    (nx::update::InformationError::httpError, "http error")
    (nx::update::InformationError::jsonError, "json error")
    (nx::update::InformationError::incompatibleCloudHost, "incompatible cloud host"))

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Package, (xml)(csv_record)(ubjson)(json), Package_Fields)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(Information, (xml)(csv_record)(ubjson)(json), Information_Fields)

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
    QString url;
    qint64 size;
    QByteArray md5;
};

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    FileData, (json),
    (file)(url)(size)(md5))

const static QString kAlternativesServersKey = "__info";

static InformationError findCustomizationInfo(
    const QJsonObject topLevelObject,
    CustomizationInfo* customizationInfo,
    Information* result)
{
    QJsonObject::const_iterator it = topLevelObject.constBegin();
    for (; it != topLevelObject.constEnd(); ++it)
    {
        if (it.key() == kAlternativesServersKey || it.key() == "__version")
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

static InformationError parseOsObject(
    const QString& osName,
    const QJsonObject& osObject,
    bool isClient,
    Information* result)
{
    for (auto it = osObject.constBegin(); it != osObject.constEnd(); ++it)
    {
        FileData fileData;
        auto targetName = it.key();
        auto osObject = it.value().toObject();
        QString fullTargetName = osName + "." + targetName;
        if (!QJson::deserialize(osObject, targetName, &fileData))
        {
            NX_ERROR(typeid(Information), lm("Target json parsing failed: %1").args(fullTargetName));
            return InformationError::jsonError;
        }

        auto archVariant = targetName.split('_');
        if (archVariant.size() != 2)
        {
            NX_ERROR(typeid(Information), lm("Target name parsing failed: %1").args(fullTargetName));
            return InformationError::jsonError;
        }

        Package package;
        package.component = isClient ? Component::client : Component::server;
        package.arch = archVariant[0];
        package.platform = archVariant[1];
        package.variant = osName;
        package.file = "update/" + fileData.file;
        package.url = fileData.url;
        package.size = fileData.size;
        package.md5 = fileData.md5;

        result->packages.append(package);
    }

    return InformationError::noError;
}

static InformationError parsePackages(
    const QJsonObject topLevelObject,
    const QString& key,
    Information* result)
{
    QJsonObject::const_iterator packagesIt = topLevelObject.find(key);
    if (packagesIt == topLevelObject.constEnd())
    {
        NX_ERROR(typeid(Information), lm("Failed to find %1").args(key));
        return InformationError::jsonError;
    }

    auto packagesObject = packagesIt.value().toObject();
    bool isClient = key.contains("client");
    for (auto osIt = packagesObject.constBegin(); osIt != packagesObject.constEnd(); ++osIt)
    {
        InformationError error = parseOsObject(
            osIt.key(),
            osIt.value().toObject(),
            isClient,
            result);
        if (error != InformationError::noError)
            return error;
    }

    return InformationError::noError;
}

static InformationError parseAndExtractInformation(const QByteArray& data, Information* result)
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

    if (result->cloudHost != nx::network::SocketGlobals::cloud().cloudHost())
        return InformationError::incompatibleCloudHost;

    InformationError error = parsePackages(topLevelObject, "packages", result);
    if (error != InformationError::noError)
        return error;

    return parsePackages(topLevelObject, "clientPackages", result);
}

static InformationError fillUpdateInformation(
    nx::network::http::AsyncClient* httpClient,
    const QString& publicationKey,
    Information* result,
    QList<AlternativeServerData>* alternativeServers)
{
    QJsonParseError parseError;
    auto topLevelObject = QJsonDocument::fromJson(
        httpClient->fetchMessageBodyBuffer(),
        &parseError).object();

    if (parseError.error != QJsonParseError::ParseError::NoError || topLevelObject.isEmpty())
        return InformationError::jsonError;

    if (QJson::deserialize(topLevelObject, kAlternativesServersKey, alternativeServers))
        return InformationError::jsonError;

    CustomizationInfo customizationInfo;
    InformationError error = findCustomizationInfo(topLevelObject, &customizationInfo, result);
    if (error != InformationError::noError)
        return error;

    error = makeHttpRequest(
        httpClient,
        customizationInfo.updates_prefix + "/" + publicationKey + "/update.json");

    if (error != InformationError::noError)
        return error;

    error = parseAndExtractInformation(httpClient->fetchMessageBodyBuffer(), result);
    if (error != InformationError::noError)
        return error;

    return InformationError::noError;
}

Information updateInformationImpl(
    const QString& url,
    const QString& publicationKey,
    InformationError* error,
    bool checkAlternativeServers)
{
    nx::network::http::AsyncClient httpClient;
    InformationError localError = makeHttpRequest(&httpClient, url);

    if (localError != InformationError::noError)
    {
        if (error)
            *error = localError;
        return Information();
    }

    Information result;
    QList<AlternativeServerData> alternativeServers;

    localError = fillUpdateInformation(&httpClient, publicationKey, &result, &alternativeServers);
    if (localError != InformationError::noError)
    {
        if (error)
            *error = localError;

        if (!checkAlternativeServers)
            return result;

        for (const auto& alternativeServer: alternativeServers)
        {
            result = updateInformationImpl(
                alternativeServer.url,
                publicationKey,
                &localError,
                /*checkAlternativeServers*/ false);

            if (localError == InformationError::noError)
            {
                if (error)
                    *error = localError;;
                return result;
            }
        }
    }

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

} // namespace update
} // namespace nx

