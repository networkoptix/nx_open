#include "update_check.h"

#include <QtCore/QJsonObject>
#include <QtCore/QThread>

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
#include <api/server_rest_connection.h>

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

const vms::api::SoftwareVersion kPackagesJsonAppearedVersion(4, 0);

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
                    lm("Http client error: %1, url: %2").args(statusCode, url));
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
    // As of 4.0 we do not use description from customization info.
    //result->description = customizationInfo->description;
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
        newPackage.file = updateFilePathForDownloader(publicationKey, p.file);
        newPackage.url = baseUpdateUrl + "/" + p.file;
        result->packages.append(newPackage);
    }

    return InformationError::noError;
}

static utils::OsInfo osInfoFromLegacySystemInformation(
    const QString& platform, const QString& arch, const QString& modification)
{
    if (modification == "bpi")
        return {"nx1"};
    if (modification == "edge1")
        return {modification};

    if (platform == "macosx")
        return {"macos"};
    if (platform == "ios")
        return {platform};

    const QString newArch = arch == "arm" ? "arm32" : arch;
    const QString newPlatform = platform + L'_' + newArch;

    static QSet<QString> kAllowedModifications{"ubuntu"};
    const QString variant =
        kAllowedModifications.contains(modification) ? modification : QString();

    return {newPlatform, variant};
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
                    const auto& osInfo = osInfoFromLegacySystemInformation(
                        itOs.key(),
                        archAndVariant[0],
                        archAndVariant.size() == 2 ? archAndVariant[1] : QString());
                    package.platform = osInfo.platform;
                    // Old update.json didn't contain variant version.
                    if (!osInfo.variant.isEmpty())
                        package.variants.append(Variant(osInfo.variant));
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
        newPackage.file = updateFilePathForDownloader(publicationKey, p.file);
        newPackage.url = baseUpdateUrl + "/" + p.file;
        result->packages.append(newPackage);
    }

    return InformationError::noError;
}

// Parses header for packages.json or update.json. Header part is mostly equal.
static InformationError parseHeader(
    const QJsonObject& topLevelObject,
    const QString& baseUpdateUrl,
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

    if (!QJson::deserialize(topLevelObject, "eula", &result->eula))
    {
        NX_WARNING(typeid(Information)) << "no eula data at" << baseUpdateUrl;
    }

    QJson::deserialize(topLevelObject, "description", &result->description);
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

    auto error = parseHeader(topLevelObject, baseUpdateUrl, result);
    if (error != InformationError::noError)
        return error;

    if (vms::api::SoftwareVersion(result->version) < kPackagesJsonAppearedVersion)
        return InformationError::incompatibleParser;

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

    auto error = parseHeader(topLevelObject, baseUpdateUrl, result);
    if (error != InformationError::noError)
        return error;

    if (vms::api::SoftwareVersion(result->version) >= kPackagesJsonAppearedVersion)
        return InformationError::incompatibleParser;

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
        error = parseAndExtractInformation(
            httpClient->fetchMessageBodyBuffer(),
            baseUpdateUrl,
            publicationKey,
            result);
        if (error != InformationError::incompatibleParser)
            return error;
    }

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
    const utils::OsInfo& osInfo,
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

    auto [result, package] = findPackageForVariant(updateInformation, isClient, osInfo);
    if (result != FindPackageResult::ok)
    {
        setErrorMessage(
            QString("Failed to find a suitable update package for %1").arg(toString(osInfo)),
            outMessage);
        return result;
    }

    *outPackage = *package;
    return FindPackageResult::ok;
}

static FindPackageResult findPackage(
    const QnUuid& moduleGuid,
    const nx::vms::api::SoftwareVersion& engineVersion,
    const utils::OsInfo& osInfo,
    const QByteArray& serializedUpdateInformation,
    bool isClient,
    const QString& cloudHost,
    bool boundToCloud,
    nx::update::Package* outPackage,
    QString* outMessage)
{
    update::Information updateInformation;
    NX_DEBUG(
        nx::utils::log::Tag(QString("UpdateCheck")),
        lm("update::findPackage: serialized update information is empty %1")
            .args(serializedUpdateInformation.isEmpty()));
    const auto deserializeResult = fromByteArray(serializedUpdateInformation,
        &updateInformation, outMessage);
    if (deserializeResult != FindPackageResult::ok)
        return deserializeResult;
    return findPackage(moduleGuid, engineVersion, osInfo, updateInformation, isClient,
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

std::pair<FindPackageResult, const Package*> findPackageForVariant(
    const nx::update::Information& updateInformation,
    bool isClient,
    const utils::OsInfo& osInfo)
{
    bool variantFound = false;
    const Package* bestPackage = nullptr;

    for (const Package& package: updateInformation.packages)
    {
        if (isClient != (package.component == kComponentClient))
            continue;

        if (package.isCompatibleTo(osInfo))
        {
            if (!bestPackage || package.isNewerThan(osInfo.variant, *bestPackage))
                bestPackage = &package;
        }
        else if (package.isCompatibleTo(osInfo, true))
        {
            variantFound = true;
        }
    }

    if (bestPackage)
        return {FindPackageResult::ok, bestPackage};
    return {
        variantFound ? FindPackageResult::osVersionNotSupported : FindPackageResult::otherError,
        nullptr};
}

std::pair<FindPackageResult, Package*> findPackageForVariant(
    Information& updateInformation,
    bool isClient,
    const utils::OsInfo& osInfo)
{
    auto [code, package] = findPackageForVariant(
        const_cast<const Information&>(updateInformation), isClient, osInfo);
    return {code, const_cast<Package*>(package)};
}

FindPackageResult findPackage(
    const QnCommonModule& commonModule,
    nx::update::Package* outPackage,
    QString* outMessage)
{
    return findPackage(
        commonModule.moduleGUID(),
        commonModule.engineVersion(),
        utils::OsInfo::current(),
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
        utils::OsInfo::current(),
        updateInformation,
        commonModule.runtimeInfoManager()->localInfo().data.peer.isClient(),
        nx::network::SocketGlobals::cloud().cloudHost(),
        !commonModule.globalSettings()->cloudSystemId().isEmpty(),
        outPackage,
        outMessage);
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

QString rootUpdatesDirectoryForDownloader()
{
    return QStringLiteral("updates/");
}

QString updatesDirectoryForDownloader(const QString& publicationKey)
{
    return rootUpdatesDirectoryForDownloader() + publicationKey + L'/';
}

QString updateFilePathForDownloader(const QString& publicationKey, const QString& fileName)
{
    return updatesDirectoryForDownloader(publicationKey) + fileName;
}

nx::update::UpdateContents checkSpecificChangesetProxied(
    rest::QnConnectionPtr connection,
    const nx::utils::SoftwareVersion& engineVersion,
    const QString& updateUrl,
    const QString& changeset)
{
    nx::update::UpdateContents contents;

    contents.changeset = changeset;
    contents.info = nx::update::updateInformation(updateUrl, engineVersion,
        changeset, &contents.error);
    contents.source = lit("%1 by serverUpdateTool for build=%2").arg(updateUrl, changeset);

    if (contents.error == nx::update::InformationError::networkError && connection)
    {
        NX_WARNING(NX_SCOPE_TAG, "Checking for updates using mediaserver as proxy");
        auto promise = std::make_shared<std::promise<bool>>();
        contents.source = lit("%1 by serverUpdateTool for build=%2 proxied by mediaserver").arg(updateUrl, changeset);
        contents.info = {};
        auto proxyCheck = promise->get_future();
        connection->checkForUpdates(changeset,
            [promise = std::move(promise), &contents](bool success,
                rest::Handle /*handle*/, rest::UpdateInformationData response)
            {
                if (success)
                {
                    if (response.error != QnRestResult::NoError)
                    {
                        NX_DEBUG(NX_SCOPE_TAG,
                            "checkSpecificChangeset: An error in response to the /ec2/updateInformation request: %1",
                            response.errorString);

                        QnLexical::deserialize(response.errorString, &contents.error);
                    }
                    else
                    {
                        contents.error = nx::update::InformationError::noError;
                        contents.info = response.data;
                    }
                }
                else
                {
                    contents.error = nx::update::InformationError::networkError;
                }

                promise->set_value(success);
            });

        proxyCheck.wait();
    }

    return contents;
}

qint64 reservedSpacePadding()
{
    // Reasoning for such constant values: QA team asked to make them so.
    constexpr qint64 kDefaultReservedSpace = 100 * 1024 * 1024;
    constexpr qint64 kWindowsReservedSpace = 500 * 1024 * 1024;
    constexpr qint64 kArmReservedSpace = 5 * 1024 * 1024;

    if (nx::utils::AppInfo::isWindows())
        return kWindowsReservedSpace;

    if (nx::utils::AppInfo::isArm())
        return kArmReservedSpace;

    return kDefaultReservedSpace;
}

} // namespace nx::update
