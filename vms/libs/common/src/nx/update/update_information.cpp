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
#include <nx/utils/scope_guard.h>

namespace {

const QString kClientComponent = "client";
const QString kServerComponent = "server";

} // namespace

namespace nx::update {

QN_DEFINE_EXPLICIT_ENUM_LEXICAL_FUNCTIONS(
    nx::update, InformationError,
    (nx::update::InformationError::noError, "no error")
    (nx::update::InformationError::networkError, "network error")
    (nx::update::InformationError::httpError, "http error")
    (nx::update::InformationError::jsonError, "json error")
    (nx::update::InformationError::brokenPackageError, "local update package is corrupted")
    (nx::update::InformationError::missingPackageError, "missing files in the update package")
    (nx::update::InformationError::incompatibleVersion, "incompatible version")
    (nx::update::InformationError::incompatibleCloudHost, "incompatible cloud host")
    (nx::update::InformationError::notFoundError, "not found")
    (nx::update::InformationError::serverConnectionError, "failed to get response from mediaserver")
    (nx::update::InformationError::noNewVersion, "no new version"))

QString toString(InformationError error)
{
    QString result;
    QnLexical::serialize(error, &result);
    return result;
}

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (Variant)(Package)(Information),
    (ubjson)(json)(datastream)(eq),
    _Fields)

QN_FUSION_ADAPT_STRUCT_FUNCTIONS_FOR_TYPES(
    (PackageInformation), (json)(datastream), _Fields)

QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(nx::update::Status, Code)
QN_DEFINE_METAOBJECT_ENUM_LEXICAL_FUNCTIONS(nx::update::Status, ErrorCode)
QN_FUSION_ADAPT_STRUCT_FUNCTIONS(nx::update::Status, (ubjson)(json), UpdateStatus_Fields)

bool Package::isServer() const
{
    return component == kServerComponent;
}

bool Package::isClient() const
{
    return component == kClientComponent;
}

bool Package::isCompatibleTo(const utils::OsInfo& osInfo, bool ignoreVersion) const
{
    return isPackageCompatibleTo(platform, variants, osInfo, ignoreVersion);
}

bool Package::isNewerThan(const QString& variant, const Package& other) const
{
    return isPackageNewerForVariant(variant, variants, other.variants);
}

bool PackageInformation::isServer() const
{
    return component == kServerComponent;
}

bool PackageInformation::isClient() const
{
    return component == kClientComponent;
}

bool PackageInformation::isCompatibleTo(const utils::OsInfo& osInfo) const
{
    return isPackageCompatibleTo(platform, variants, osInfo);
}

void UpdateContents::resetVerification()
{
    error = nx::update::InformationError::noError;
    for (auto& package: info.packages)
    {
        package.targets.clear();
        package.localFile.clear();
    }

    filesToUpload.clear();
    missingUpdate.clear();
    invalidVersion.clear();
    ignorePeers.clear();

    unsuportedSystemsReport.clear();
    peersWithUpdate.clear();
    manualPackages.clear();
    packagesGenerated = false;
}

nx::utils::SoftwareVersion UpdateContents::getVersion() const
{
    return nx::utils::SoftwareVersion(info.version);
}

bool UpdateContents::isValidToInstall() const
{
    return missingUpdate.empty()
        && unsuportedSystemsReport.empty()
        && !info.version.isEmpty()
        && invalidVersion.empty()
        && error == nx::update::InformationError::noError;
}

bool UpdateContents::isEmpty() const
{
    return info.packages.empty() || info.version.isEmpty();
}

bool UpdateContents::preferOtherUpdate(const UpdateContents& other) const
{
    if (isEmpty())
        return true;

    // Prefere update from mediaservers.
    if (sourceType != UpdateSourceType::mediaservers
        && other.sourceType == UpdateSourceType::mediaservers)
    {
        return true;
    }
    else if (sourceType != UpdateSourceType::mediaservers
        && other.sourceType == UpdateSourceType::mediaservers)
    {
        return false;
    }

    return other.getVersion() > getVersion();
}

bool isPackageCompatibleTo(
    const QString& packagePlatform,
    const QList<Variant>& packageVariants,
    const utils::OsInfo& osInfo,
    bool ignoreVersion)
{
    if (packagePlatform != osInfo.platform)
        return false;

    if (packageVariants.isEmpty())
        return true;

    const utils::SoftwareVersion currentVersion(osInfo.variantVersion);

    for (const Variant& variant: packageVariants)
    {
        if (variant.name != osInfo.variant)
            continue;

        if (ignoreVersion)
            return true;

        if (!variant.minimumVersion.isEmpty()
            && utils::SoftwareVersion(variant.minimumVersion) > currentVersion)
        {
            return false;
        }
        if (!variant.maximumVersion.isEmpty()
            && utils::SoftwareVersion(variant.maximumVersion) < currentVersion)
        {
            return false;
        }

        return true;
    }

    return false;
}

bool isPackageNewerForVariant(
    const QString& variant,
    const QList<Variant>& packageVariants,
    const QList<Variant>& otherPackageVariants)
{
    const auto it = std::find_if(packageVariants.begin(), packageVariants.end(),
        [&variant](const Variant& v) { return v.name == variant; });
    if (it == packageVariants.end())
        return false;

    const auto itOther = std::find_if(otherPackageVariants.begin(), otherPackageVariants.end(),
        [&variant](const Variant& v) { return v.name == variant; });
    if (itOther == otherPackageVariants.end())
        return true;

    return utils::SoftwareVersion(it->minimumVersion)
        >= utils::SoftwareVersion(itOther->minimumVersion);
}

} // namespace nx::update
