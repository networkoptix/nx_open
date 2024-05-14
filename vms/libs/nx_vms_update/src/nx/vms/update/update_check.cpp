// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "update_check.h"

#include <algorithm>

#include <QtCore/QJsonObject>

#include <nx/fusion/serialization/json_functions.h>
#include <nx/network/http/http_client.h>
#include <nx/utils/log/log.h>

using namespace std::chrono;

namespace nx::vms::update {

namespace {

constexpr milliseconds kFetchBodyTimeout = 1min;
const nx::utils::SoftwareVersion kPackagesJsonAppearedVersion(4, 0);

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
    const QString newPlatform = platform + '_' + newArch;

    static QSet<QString> kAllowedModifications{"ubuntu"};
    const QString variant =
        kAllowedModifications.contains(modification) ? modification : QString();

    return {newPlatform, variant};
}

template<typename T>
std::variant<T, FetchError> fetchJson(const nx::utils::Url& url)
{
    nx::network::http::HttpClient httpClient{nx::network::ssl::kDefaultCertificateCheck};

    if (!httpClient.doGet(url))
    {
        NX_WARNING(NX_SCOPE_TAG, "Request failed. URL: %1, Code: %2",
            url, SystemError::toString(httpClient.lastSysErrorCode()));
        return FetchError::networkError;
    }

    if (const auto status = httpClient.response()->statusLine.statusCode;
        status != nx::network::http::StatusCode::ok)
    {
        NX_WARNING(NX_SCOPE_TAG, "Request failed. URL: %1, HTTP status: %2", url, status);
        return FetchError::httpError;
    }

    const std::optional<nx::Buffer> body = httpClient.fetchEntireMessageBody(kFetchBodyTimeout);
    if (!body)
    {
        NX_WARNING(NX_SCOPE_TAG, "Timeout expired for request. URL: %1", url);
        return FetchError::httpError;
    }

    bool ok = false;
    const auto info = QJson::deserialized<T>(*body, {}, &ok);
    if (!ok)
    {
        NX_WARNING(NX_SCOPE_TAG, "Cannot deserialize reply. URL: %1", url);
        return FetchError::parseError;
    }

    return info;
}

} // namespace

FetchReleasesResult fetchReleasesInfo(const nx::utils::Url& url)
{
    return fetchJson<ReleasesInfo>(url);
}

FetchPublicationInfoResult fetchPublicationInfo(
    const nx::utils::SoftwareVersion& version, const nx::utils::Url& urlPrefix)
{
    const utils::SoftwareVersion kFullVersionUrlsAppearedVersion(4, 2);

    auto effectiveUrl = urlPrefix;

    FetchPublicationInfoResult result = FetchError::httpError;
    if (version.major != 0)
    {
        // Check by full version.
        effectiveUrl.setPath(urlPrefix.path() + nx::format("/%1/packages.json", version));
        result = fetchJson<PublicationInfo>(effectiveUrl);
    }

    if (const auto error = std::get_if<FetchError>(&result);
        error && *error == FetchError::httpError && version < kFullVersionUrlsAppearedVersion)
    {
        // Fallback to build-number-only URL.
        effectiveUrl.setPath(urlPrefix.path() + nx::format("/%1/packages.json", version.build));
        result = fetchJson<PublicationInfo>(effectiveUrl);
    }

    if (auto publication = std::get_if<PublicationInfo>(&result))
        publication->url = effectiveUrl;

    return result;
}

FetchPublicationInfoResult fetchPublicationInfo(
    const nx::utils::SoftwareVersion& version, const QVector<nx::utils::Url>& urlPrefixes)
{
    if (!NX_ASSERT(!urlPrefixes.isEmpty()))
        return FetchError::paramsError;

    std::optional<PublicationInfo> info;
    std::optional<FetchError> firstError;

    for (const auto& prefix: urlPrefixes)
    {
        const auto& result = fetchPublicationInfo(version, prefix);
        if (const auto pubInfo = std::get_if<PublicationInfo>(&result))
        {
            const QString publicationUrlPath = pubInfo->url.path().remove("packages.json");

            if (!info)
            {
                info = *pubInfo;
                info->packages = {};
            }

            for (Package package: pubInfo->packages)
            {
                if (!std::any_of(info->packages.begin(), info->packages.end(),
                    [package](const Package& p) { return package.isSameTarget(p); }))
                {
                    if (!package.url.isValid())
                    {
                        package.url = pubInfo->url;
                        package.url.setPath(publicationUrlPath + package.file);
                    }
                    info->packages.append(package);
                }
            }
        }
        else
        {
            if (!firstError)
                firstError = std::get<FetchError>(result);
        }
    }

    if (info)
        return *info;
    else
        return *firstError;
}

FetchPublicationInfoResult fetchLegacyPublicationInfo(
    const nx::utils::SoftwareVersion& version, const nx::utils::Url& urlPrefix)
{
    // Legacy versions don't have full version in URLs.
    auto publicationUrlPath = urlPrefix.path() + nx::format("/%1/", version.build);

    auto effectiveUrl = urlPrefix;
    effectiveUrl.setPath(publicationUrlPath + "update.json");
    const auto fetchResult = fetchJson<QJsonObject>(effectiveUrl);
    if (std::holds_alternative<FetchError>(fetchResult))
        return std::get<FetchError>(fetchResult);

    const auto& json = std::get<QJsonObject>(fetchResult);

    static const auto& getField =
        [](const QJsonObject& json, const QString& key)
        {
            if (!json.contains(key))
            {
                NX_WARNING(NX_SCOPE_TAG, "Legacy info does not contain key %1", key);
                throw std::exception();
            }
            return json.value(key);
        };

    static const auto& getStringField =
        [](const QJsonObject& json, const QString& key)
        {
            const QJsonValue result = getField(json, key);
            if (result.type() != QJsonValue::String)
            {
                NX_WARNING(NX_SCOPE_TAG, "Unexpected value type for key %1", key);
                throw std::exception();
            }

            return result.toString();
        };

    static const auto& getIntField =
        [](const QJsonObject& json, const QString& key)
        {
            const QJsonValue result = getField(json, key);
            if (result.type() != QJsonValue::Double)
            {
                NX_WARNING(NX_SCOPE_TAG, "Unexpected value type for key %1", key);
                throw std::exception();
            }

            return (qint64) result.toDouble();
        };

    static const auto& readPackages =
        [urlPrefix, publicationUrlPath](const QJsonObject& json, const Component& component)
        {
            QList<Package> result;

            for (auto itOs = json.begin(); itOs != json.end(); ++itOs)
            {
                if (!itOs->isObject())
                    throw std::exception();

                const auto os = itOs->toObject();
                for (auto itArch = os.begin(); itArch != os.end(); ++itArch)
                {
                    if (!itArch->isObject())
                        throw std::exception();

                    // Arch actually consists of arch_variant.
                    const QStringList archAndVariant = itArch.key().split("_");
                    if (archAndVariant.empty())
                        throw std::exception();

                    const nx::utils::OsInfo osInfo = osInfoFromLegacySystemInformation(
                        itOs.key(),
                        archAndVariant[0],
                        archAndVariant.size() == 2 ? archAndVariant[1] : QString());

                    const auto packageJson = itArch->toObject();

                    Package package;
                    package.component = component;
                    package.file = getStringField(packageJson, "file");
                    package.md5 = getStringField(packageJson, "md5");
                    package.size = getIntField(packageJson, "size");
                    package.platform = osInfo.platform;
                    package.url = urlPrefix;
                    package.url.setPath(publicationUrlPath + package.file);
                    // Old update.json doesn't contain variant version.
                    if (!osInfo.variant.isEmpty())
                        package.variants.append(PlatformVariant{.name = osInfo.variant});

                    result.append(package);
                }
            }
            return result;
        };

    PublicationInfo info;

    try
    {
        info.version = nx::utils::SoftwareVersion(getStringField(json, "version"));
        if (info.version.isNull())
            throw std::exception();
        info.cloudHost = getStringField(json, "cloudHost");
        info.packages =
            readPackages(getField(json, "clientPackages").toObject(), Component::client);
        info.packages +=
            readPackages(getField(json, "packages").toObject(), Component::server);
        info.url = effectiveUrl;

    }
    catch (const std::exception&)
    {
        return FetchError::parseError;
    }

    return info;
}

FetchPublicationInfoResult fetchLegacyPublicationInfo(
    const nx::utils::SoftwareVersion& version, const QVector<nx::utils::Url>& urlPrefixes)
{
    if (!NX_ASSERT(!urlPrefixes.isEmpty()))
        return FetchError::paramsError;

    // Unlike fetchPublicationInfo(), this function does not merge packages from all prefixes.
    // It should just return first found info.
    std::optional<FetchError> firstError;

    for (const auto& prefix: urlPrefixes)
    {
        const auto fetchResult = fetchLegacyPublicationInfo(version, prefix);
        if (std::holds_alternative<PublicationInfo>(fetchResult))
            return fetchResult;
        else if (!firstError)
            firstError = std::get<FetchError>(fetchResult);
    }

    return *firstError;
}

static PublicationInfoResult getCertainVersionPublicationInfo(
    const nx::utils::SoftwareVersion& version,
    const QVector<nx::utils::Url>& urlPrexifes)
{
    // When we have only publication key, we don't know if it's a legacy package or new package, so
    // try both variants.

    const auto fetchResult = fetchPublicationInfo(version, urlPrexifes);
    if (std::holds_alternative<PublicationInfo>(fetchResult))
        return std::get<PublicationInfo>(fetchResult);

    // Retry only in case of HTTP error, because for a missing `packages.json` for a valid package
    // we expect HTTP error 404 or alike.
    if (std::get<FetchError>(fetchResult) != FetchError::httpError)
        return std::get<FetchError>(fetchResult);

    const auto fetchLegacyInfoResult = fetchLegacyPublicationInfo(version, urlPrexifes);
    if (std::holds_alternative<PublicationInfo>(fetchLegacyInfoResult))
        return std::get<PublicationInfo>(fetchLegacyInfoResult);
    return std::get<FetchError>(fetchLegacyInfoResult);
}

PublicationInfoResult getPublicationInfo(
    const nx::utils::Url& url, const PublicationInfoParams& params)
{
    const auto releasesResult = fetchReleasesInfo(url);
    if (std::holds_alternative<FetchError>(releasesResult))
        return std::get<FetchError>(releasesResult);

    const auto releasesInfo = std::get<ReleasesInfo>(releasesResult);
    std::optional<ReleaseInfo> release;

    if (std::holds_alternative<LatestVmsVersionParams>(params))
    {
        const auto& p = std::get<LatestVmsVersionParams>(params);
        release = releasesInfo.selectVmsRelease(p.currentVersion);
    }
    else if (std::holds_alternative<LatestDesktopClientVersionParams>(params))
    {
        const auto& p = std::get<LatestDesktopClientVersionParams>(params);
        release = releasesInfo.selectDesktopClientRelease(
            p.currentVersion, p.publicationType, p.protocolVersion);
    }
    else if (std::holds_alternative<CertainVersionParams>(params))
    {
        const auto& p = std::get<CertainVersionParams>(params);
        return getCertainVersionPublicationInfo(p.version, releasesInfo.packages_urls);
    }

    if (!release)
        return nullptr;

    PublicationInfo publicationInfo;

    if (release->version >= kPackagesJsonAppearedVersion)
    {
        const auto fetchResult = fetchPublicationInfo(
            release->version, releasesInfo.packages_urls);
        if (std::holds_alternative<FetchError>(fetchResult))
            return std::get<FetchError>(fetchResult);

        publicationInfo = std::get<PublicationInfo>(fetchResult);
    }
    else
    {
        const auto fetchResult = fetchLegacyPublicationInfo(
            release->version, releasesInfo.packages_urls);
        if (std::holds_alternative<FetchError>(fetchResult))
            return std::get<FetchError>(fetchResult);

        publicationInfo = std::get<PublicationInfo>(fetchResult);
    }

    publicationInfo.releaseDate = milliseconds(release->release_date);
    publicationInfo.releaseDeliveryDays = release->release_delivery_days;

    return publicationInfo;
}

} // namespace nx::vms::update
