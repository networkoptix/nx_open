#include "update_check.h"
#include <QJsonObject>
#include <nx/fusion/model_functions.h>
#include <nx/network/http/http_async_client.h>
#include <nx/utils/std/future.h>
#include <nx/utils/log/log.h>
#include <utils/common/app_info.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/vms/api/data/software_version.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/app_info.h>

#include <common/static_common_module.h>

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
    if (!QJson::deserialize(topLevelObject, QnAppInfo::customizationName(), customizationInfo))
    {
        NX_ERROR(
            typeid(Information),
            lm("Customization %1 not found").args(QnAppInfo::customizationName()));

        return InformationError::jsonError;
    }

    result->releaseNotesUrl = customizationInfo->release_notes;
    return InformationError::noError;
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

    auto packagesError = parsePackages(topLevelObject, baseUpdateUrl, publicationKey, result);
    if (packagesError != InformationError::noError)
        return packagesError;

    if (!QJson::deserialize(topLevelObject, "version", &result->version))
        return InformationError::jsonError;

    if (!QJson::deserialize(topLevelObject, "cloudHost", &result->cloudHost))
    {
        NX_WARNING(typeid(Information), "no cloudHost at %1", baseUpdateUrl);
    }

    if (!QJson::deserialize(topLevelObject, "eulaVersion", &result->eulaVersion))
    {
        NX_WARNING(typeid(Information)) << "no eulaVersion at" << baseUpdateUrl;
    }

    if(!QJson::deserialize(topLevelObject, "eulaLink", &result->eulaLink))
    {
        NX_WARNING(typeid(Information)) << "no eulaLink at" << baseUpdateUrl;
    }

    return InformationError::noError;
}

static InformationError fillUpdateInformation(
    nx::network::http::AsyncClient* httpClient,
    QString publicationKey,
    nx::vms::api::SoftwareVersion currentVersion,
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

    if (publicationKey == "latest")
    {
        // Extracting latest version from
        QString version = currentVersion.toString(nx::vms::api::SoftwareVersion::MinorFormat);
        auto it = customizationInfo.releases.find(version);
        if (it == customizationInfo.releases.end())
            return InformationError::noNewVersion;
        publicationKey = it.value().build();
        NX_INFO(typeid(Information)) << "fillUpdateInformation will use build" << publicationKey;
    }

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
    nx::vms::api::SoftwareVersion currentVersion,
    InformationError* error,
    bool checkAlternativeServers)
{
    nx::network::http::AsyncClient httpClient;
    Information result;
    InformationError localError = makeHttpRequest(&httpClient, url);
    auto onStop = nx::utils::makeScopeGuard(
        [&httpClient](){ httpClient.pleaseStopSync(); });

    if (localError != InformationError::noError)
    {
        if (error)
            *error = localError;
        return result;
    }

    QList<AlternativeServerData> alternativeServers;
    localError = fillUpdateInformation(
        &httpClient,
        publicationKey,
        currentVersion,
        &result, &alternativeServers);

    if (localError != InformationError::noError && checkAlternativeServers)
    {
        for (const auto& alternativeServer: alternativeServers)
        {
            result = updateInformationImpl(
                alternativeServer.url,
                publicationKey, currentVersion,
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
    auto version = qnStaticCommon->engineVersion();
    return updateInformationImpl(url, publicationKey, version, error, /*checkAlternativeServers*/ true);
}

Information updateInformation(const QString& /*zipFileName*/, InformationError* /*error*/)
{
    // TODO: Implement it.
    // Right now ServerUpdateTool deals with zip package.
    return Information();
}

static void setErrorMessage (const QString& message, QString* outMessage)
{
    if (outMessage)
        *outMessage = message;
};

FindPackageResult findPackage(
    const vms::api::SystemInformation& systemInformation,
    const nx::update::Information& updateInformation,
    bool isClient,
    const QString& cloudHost,
    bool boundToCloud,
    nx::update::Package* outPackage,
    QString* outMessage)
{
    if (updateInformation.isEmpty())
    {
        setErrorMessage("Update information is empty", outMessage);
        return FindPackageResult::noInfo;
    }

    if (updateInformation.cloudHost != cloudHost && boundToCloud)
    {
        setErrorMessage(QString::fromLatin1(
            "Peer cloud host (%1) doesn't match update cloud host (%2)")
                .arg(cloudHost)
                .arg(updateInformation.cloudHost),
            outMessage);
        return FindPackageResult::otherError;
    }

    if (nx::utils::SoftwareVersion(updateInformation.version) <= qnStaticCommon->engineVersion())
    {
        setErrorMessage(QString::fromLatin1(
            "The update application version (%1) is less or equal to the peer application " \
            "version (%2)")
                .arg(updateInformation.version)
                .arg(qnStaticCommon->engineVersion().toString()),
            outMessage);
        return FindPackageResult::otherError;
    }

    for (const auto& package : updateInformation.packages)
    {
        if (isClient != (package.component == update::kComponentClient))
            continue;

        nx::utils::SoftwareVersion selfOsVariant(systemInformation.version);
        nx::utils::SoftwareVersion packageOsVariant(package.variantVersion);

        if (package.arch == systemInformation.arch
            && package.platform == systemInformation.platform
            && package.variant == systemInformation.modification
            && (packageOsVariant <= selfOsVariant || selfOsVariant.isNull()))
        {
            *outPackage = package;
            return FindPackageResult::ok;
        }
    }

    setErrorMessage(QString::fromLatin1(
        "Failed to find a suitable update package for arch %1, platform %2, " \
        "modification (variant) %3, version %4")
            .arg(systemInformation.arch)
            .arg(systemInformation.platform)
            .arg(systemInformation.modification)
            .arg(systemInformation.version),
        outMessage);

    return FindPackageResult::otherError;
}

FindPackageResult findPackage(
    const vms::api::SystemInformation& systemInformation,
    const QByteArray& serializedUpdateInformation,
    bool isClient,
    const QString& cloudHost,
    bool boundToCloud,
    nx::update::Package* outPackage,
    QString* outMessage)
{
    if (serializedUpdateInformation.isEmpty())
    {
        setErrorMessage("Update information is empty", outMessage);
        return FindPackageResult::noInfo;
    }

    update::Information updateInformation;
    if (!QJson::deserialize(serializedUpdateInformation, &updateInformation))
    {
        setErrorMessage("Failed to deserialize update information JSON", outMessage);
        return FindPackageResult::otherError;
    }

    return findPackage(systemInformation, updateInformation, isClient, cloudHost, boundToCloud,
        outPackage, outMessage);
}

} // namespace update
} // namespace nx

