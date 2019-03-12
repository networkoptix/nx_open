#include "update_check.h"
#include <QJsonObject>
#include <nx/fusion/model_functions.h>
#include <nx/network/http/http_async_client.h>
#include <nx/utils/std/future.h>
#include <nx/utils/log/log.h>
#include <utils/common/app_info.h>
#include <utils/common/delayed.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/vms/api/data/software_version.h>
#include <nx/utils/scope_guard.h>
#include <nx/utils/app_info.h>
#include <common/common_module.h>
#include <api/global_settings.h>
#include <nx/network/socket_global.h>
#include <api/runtime_info_manager.h>

namespace nx::update {

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
    qint64 release_date = 0;
    int release_delivery = 0;
    QMap<QString, nx::vms::api::SoftwareVersion> releases;
};

QN_FUSION_ADAPT_STRUCT_FUNCTIONS(
    CustomizationInfo,
    (json),
    (current_release)(updates_prefix)(release_notes)(release_date)(release_delivery)(description)(releases))

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
            auto state = httpClient->state();
            if (state != nx::network::http::AsyncClient::State::sDone)
            {
                error = InformationError::networkError;
                auto code = SystemError::toString(httpClient->lastSysErrorCode());
                NX_WARNING(typeid(Information),
                    "Failed to get update info from the internet, url: %1, code=%2", url, code);
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
    result->releaseDeliveryDays = customizationInfo->release_delivery;
    result->releaseDateMs = customizationInfo->release_date;
    result->description = customizationInfo->description;
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

static InformationError parseLegacyPackages(
    const QJsonObject topLevelObject,
    const QString& baseUpdateUrl,
    const QString& publicationKey,
    Information* result)
{
    // Legacy update.json has two core package maps: "packages" and "clientPackages":
    //  OS:
    //      arch:
    QList<Package> packages;

    if (!topLevelObject.contains("packages"))
        return InformationError::brokenPackageError;

    if (!topLevelObject.contains("clientPackages"))
        return InformationError::brokenPackageError;

    auto readPackages =
        [](const QString& component, const QJsonValue& source, QList<Package>& output)
        {
            if (!source.isObject())
                return InformationError::jsonError;
            QJsonObject packages = source.toObject();
            for (auto itOs = packages.begin(); itOs != packages.end(); ++itOs)
            {
                if (!itOs->isObject())
                    return InformationError::jsonError;

                auto os = itOs->toObject();
                for (auto itArch = os.begin(); itArch != os.end(); ++itArch)
                {
                    if (!itArch->isObject())
                        return InformationError::jsonError;

                    // Arch actually consists of arch_variant.
                    QStringList archAndVariant = itArch.key().split("_");
                    if (archAndVariant.empty())
                        return InformationError::jsonError;

                    Package package;
                    // It should fill in file, md5 and size fields.
                    if (!QJson::deserialize(itArch.value(), &package))
                        return InformationError::jsonError;

                    package.component = component;
                    package.arch = archAndVariant[0];
                    package.platform = itOs.key();
                    package.variant = archAndVariant.size() == 2 ? archAndVariant[1] : QString();
                    // TODO: Do we need to do anything with variantVersion? It does not seem to be used.
                    output.append(package);
                }
            }

            return InformationError::noError;
        };

    auto serverPackages = topLevelObject.value("packages");
    auto error = readPackages("server", serverPackages, packages);
    if (error != InformationError::noError)
        return error;

    auto clientPackages = topLevelObject.value("clientPackages");
    error = readPackages("client", clientPackages, packages);
    if (error != InformationError::noError)
        return error;

    for (const auto& p: packages)
    {
        Package newPackage = p;
        newPackage.file = "updates/" + publicationKey + '/' + p.file;
        newPackage.url = baseUpdateUrl + "/" + p.file;
        result->packages.append(newPackage);
    }

    return InformationError::noError;
}

// Parses header for packages.json or update.json. Header part is mostly equal.
static InformationError parseHeader(
    const QJsonObject& topLevelObject,
    const QString& baseUpdateUrl,
    bool isLegacyData,
    Information* result)
{
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

    if (!QJson::deserialize(topLevelObject, "eulaLink", &result->eulaLink))
    {
        NX_WARNING(typeid(Information)) << "no eulaLink at" << baseUpdateUrl;
    }

    if (!QJson::deserialize(topLevelObject, "eula", &result->eula))
    {
        NX_WARNING(typeid(Information)) << "no eula data at" << baseUpdateUrl;
    }

    QString description;
    // Legacy updates: e take update's description from the root updates.json and
    // override it by description in updates.json.
    // New updates: we take description only from packages.json.
    if ((QJson::deserialize(topLevelObject, "description", &description) && !description.isEmpty())
        || !isLegacyData)
    {
        result->description = description;
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

    auto error = parseHeader(topLevelObject, baseUpdateUrl, /*isLegacyData=*/false, result);
    if (error != InformationError::noError)
        return error;

    error = parsePackages(topLevelObject, baseUpdateUrl, publicationKey, result);
    if (error != InformationError::noError)
        return error;

    return InformationError::noError;
}

static InformationError parseAndExtractLegacyInformation(
    const QByteArray& data,
    const QString& baseUpdateUrl,
    const QString& publicationKey,
    Information* result)
{
    QJsonParseError parseError;
    auto topLevelObject = QJsonDocument::fromJson(data, &parseError).object();
    if (parseError.error != QJsonParseError::ParseError::NoError || topLevelObject.isEmpty())
        return InformationError::jsonError;

    auto error = parseHeader(topLevelObject, baseUpdateUrl, /*isLegacyData=*/true, result);
    if (error != InformationError::noError)
        return error;

    error = parseLegacyPackages(topLevelObject, baseUpdateUrl, publicationKey, result);
    if (error != InformationError::noError)
        return error;

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
    {
        NX_VERBOSE(typeid(Information))
            << "fillUpdateInformation no" << kAlternativesServersKey
            << "key found. I will not check alternative servers.";
    }

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
        auto updateVersion = it.value();
        publicationKey = QString::number(updateVersion.build());
        NX_VERBOSE(typeid(Information))
            << "fillUpdateInformation will use version"
            << updateVersion.toString(nx::vms::api::SoftwareVersion::FullFormat)
            << "build" << publicationKey;
    }

    auto baseUpdateUrl = customizationInfo.updates_prefix + "/" + publicationKey;
    error = makeHttpRequest(httpClient, baseUpdateUrl + "/packages.json");

    if (error == InformationError::noError)
    {
        return parseAndExtractInformation(
            httpClient->fetchMessageBodyBuffer(),
            baseUpdateUrl,
            publicationKey,
            result);
    }

    // Falling back to previous protocol with update.json
    if (error == InformationError::httpError)
        error = makeHttpRequest(httpClient, baseUpdateUrl + "/update.json");

    if (error == InformationError::noError)
    {
        return parseAndExtractLegacyInformation(
            httpClient->fetchMessageBodyBuffer(),
            baseUpdateUrl,
            publicationKey,
            result);
    }

    return error;
}

static Information updateInformationImpl(
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

static void setErrorMessage (const QString& message, QString* outMessage)
{
    if (outMessage)
        *outMessage = message;
};

static FindPackageResult findPackage(
    const QnUuid& moduleGuid,
    const nx::vms::api::SoftwareVersion& engineVersion,
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

    if (!isClient && !updateInformation.participants.isEmpty()
        && !updateInformation.participants.contains(moduleGuid))
    {
        setErrorMessage("This module is not listed in the update participants", outMessage);
        return FindPackageResult::notParticipant;
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

    if (nx::utils::SoftwareVersion(updateInformation.version) == engineVersion && !isClient)
    {
        setErrorMessage(QString::fromLatin1(
            "Latest update (%1) installed")
                .arg(engineVersion.toString()),
            outMessage);
        return FindPackageResult::latestUpdateInstalled;
    }

    if (nx::utils::SoftwareVersion(updateInformation.version) < engineVersion && !isClient)
    {
        setErrorMessage(QString::fromLatin1(
            "The update application version (%1) is less than the peer application version (%2)")
                .arg(updateInformation.version)
                .arg(engineVersion.toString()),
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
            && (package.variant == systemInformation.modification || package.variant.isNull())
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

static FindPackageResult findPackage(
    const QnUuid& moduleGuid,
    const nx::vms::api::SoftwareVersion& engineVersion,
    const vms::api::SystemInformation& systemInformation,
    const QByteArray& serializedUpdateInformation,
    bool isClient,
    const QString& cloudHost,
    bool boundToCloud,
    nx::update::Package* outPackage,
    QString* outMessage)
{
    update::Information updateInformation;
    const auto deserializeResult = fromByteArray(serializedUpdateInformation,
        &updateInformation, outMessage);
    if (deserializeResult != FindPackageResult::ok)
        return deserializeResult;
    return findPackage(moduleGuid, engineVersion, systemInformation, updateInformation, isClient,
        cloudHost, boundToCloud, outPackage, outMessage);
}

} // namespace

Information updateInformation(
    const QString& url,
    const nx::vms::api::SoftwareVersion& engineVersion,
    const QString& publicationKey,
    InformationError* error)
{
    return updateInformationImpl(url, publicationKey, engineVersion, error,
        /*checkAlternativeServers*/ true);
}

FindPackageResult fromByteArray(
    const QByteArray& serializedUpdateInformation,
    Information* outInformation,
    QString* outMessage)
{
    if (serializedUpdateInformation.isEmpty())
    {
        setErrorMessage("Update information is empty", outMessage);
        return FindPackageResult::noInfo;
    }
    if (!QJson::deserialize(serializedUpdateInformation, outInformation))
    {
        setErrorMessage("Failed to deserialize update information JSON", outMessage);
        return FindPackageResult::otherError;
    }
    return FindPackageResult::ok;
}

FindPackageResult findPackage(
    const QnCommonModule& commonModule,
    nx::update::Package* outPackage,
    QString* outMessage)
{
    return findPackage(
        commonModule.moduleGUID(),
        commonModule.engineVersion(),
        QnAppInfo::currentSystemInformation(),
        commonModule.globalSettings()->targetUpdateInformation(),
        commonModule.runtimeInfoManager()->localInfo().data.peer.isClient(),
        nx::network::SocketGlobals::cloud().cloudHost(),
        !commonModule.globalSettings()->cloudSystemId().isEmpty(),
        outPackage,
        outMessage);
}

FindPackageResult findPackage(
    const QnCommonModule& commonModule,
    const Information& updateInformation,
    nx::update::Package* outPackage,
    QString* outMessage)
{
    return findPackage(
        commonModule.moduleGUID(),
        commonModule.engineVersion(),
        QnAppInfo::currentSystemInformation(),
        updateInformation,
        commonModule.runtimeInfoManager()->localInfo().data.peer.isClient(),
        nx::network::SocketGlobals::cloud().cloudHost(),
        !commonModule.globalSettings()->cloudSystemId().isEmpty(),
        outPackage,
        outMessage);
}


nx::update::Package* findPackage(
    const QString& component,
    nx::vms::api::SystemInformation& systemInfo,
    nx::update::Information& info)
{
    for(auto& pkg: info.packages)
    {
        if (pkg.component == component)
        {
            // Check arch and OS
            if (pkg.arch == systemInfo.arch
                && pkg.platform == systemInfo.platform
                && pkg.variant == systemInfo.modification)
                return &pkg;
        }
    }
    return nullptr;
}

std::future<UpdateContents> checkLatestUpdate(
    const QString& updateUrl,
    const nx::vms::api::SoftwareVersion& engineVersion,
    UpdateCheckCallback&& callback)
{
    return std::async(std::launch::async,
        [updateUrl, callback = std::move(callback), thread = QThread::currentThread(),
        engineVersion]()
        {
            UpdateContents result;
            result.info = nx::update::updateInformation(updateUrl, engineVersion,
                nx::update::kLatestVersion, &result.error);
            result.sourceType = UpdateSourceType::internet;
            result.source = lit("%1 for build=%2").arg(updateUrl, nx::update::kLatestVersion);
            if (callback)
            {
                executeInThread(thread,
                    [callback = std::move(callback), result]()
                    {
                        callback(result);
                    });
            }
            return result;
        });
}

std::future<UpdateContents> checkSpecificChangeset(
    const QString& updateUrl,
    const nx::vms::api::SoftwareVersion& engineVersion,
    const QString& build,
    UpdateCheckCallback&& callback)
{
    return std::async(std::launch::async,
        [updateUrl, build, callback = std::move(callback), thread = QThread::currentThread(),
        engineVersion]()
        {
            UpdateContents result;
            result.info = nx::update::updateInformation(updateUrl, engineVersion, build,
                &result.error);
            result.sourceType = UpdateSourceType::internetSpecific;
            result.source = lit("%1 for build=%2").arg(updateUrl, build);
            if (result.info.version.isEmpty())
                result.info.version = build;

            if (callback)
            {
                executeInThread(thread,
                    [callback = std::move(callback), result]()
                    {
                        callback(result);
                    });
            }
            return result;
        });
}

} // namespace nx::update
