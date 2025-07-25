// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_compatibility_validator.h"

#include <nx/branding.h>
#include <nx/build_info.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/api/data/site_information.h>
#include <nx/vms/api/protocol_version.h>
#include <nx_ec/ec_api_common.h>

using namespace nx::vms::api;

namespace nx::vms::common {

using Peer = ServerCompatibilityValidator::Peer;
using Purpose = ServerCompatibilityValidator::Purpose;
using DeveloperFlag = ServerCompatibilityValidator::DeveloperFlag;
using DeveloperFlags = ServerCompatibilityValidator::DeveloperFlags;

namespace {

static Peer s_localPeerType = Peer::notDefined;
static Qn::SerializationFormat s_serializationFormat = Qn::SerializationFormat::unsupported;
static DeveloperFlags s_developerFlags = {};

static const QMap<Purpose, nx::utils::SoftwareVersion> kMinimalVersionByPurpose{
    {Purpose::connect, {4, 0}},
    {Purpose::merge, {5, 0}},
    {Purpose::connectInCompatibilityMode, {4, 0}},
    {Purpose::connectInCrossSystemMode, {5, 0}},
};

constexpr nx::utils::SoftwareVersion kMinimalMobileVersion = {5, 0};

void ensureInitialized()
{
    NX_ASSERT(ServerCompatibilityValidator::isInitialized());
}

bool compatibleCloudHost(const QString& remoteCloudHost)
{
    if (remoteCloudHost.isEmpty())
        return true;

    if (s_developerFlags.testFlag(DeveloperFlag::ignoreCloudHost))
        return true;

    auto activeCloud = nx::network::SocketGlobals::cloud().cloudHost();
    if (remoteCloudHost.toStdString() == activeCloud)
        return true;

    if (s_localPeerType == Peer::mobileClient)
    {
        // Ignore incompatible cloud host for non-release versions because Mobile Client does not
        // have compatibility versions mechanism. Thus without it beta testers will have
        // to manually downgrade the app to connect to their release systems.
        if (nx::build_info::publicationType() != nx::build_info::PublicationType::release)
            return true;

        return nx::branding::compatibleCloudHosts().contains(remoteCloudHost);
    }

    return false;
}

std::optional<ServerCompatibilityValidator::Reason> checkInternal(
    const QString& customization,
    int protoVersion,
    const nx::utils::SoftwareVersion& version,
    const QString& cloudHost,
    ServerCompatibilityValidator::Purpose purpose)
{
    ensureInitialized();

    const auto minVersion = s_localPeerType == Peer::mobileClient
        ? kMinimalMobileVersion
        : kMinimalVersionByPurpose[purpose];

    if (version < minVersion)
        return ServerCompatibilityValidator::Reason::versionIsTooLow;

    if (!ServerCompatibilityValidator::isCompatibleCustomization(customization))
        return ServerCompatibilityValidator::Reason::customizationDiffers;

    const bool compatibilityMode = (purpose == Purpose::connectInCompatibilityMode);

    if (!compatibilityMode && !compatibleCloudHost(cloudHost))
        return ServerCompatibilityValidator::Reason::cloudHostDiffers;

    // Binary protocol requires the same protocol version.
    const bool ensureProtocolVersion =
        !s_developerFlags.testFlag(DeveloperFlag::ignoreProtocolVersion)
        && !compatibilityMode
        && (s_serializationFormat == Qn::SerializationFormat::ubjson
            || api::canPeerUseUbjson(s_localPeerType))
        && (purpose != Purpose::connectInCrossSystemMode);

    if (ensureProtocolVersion && protoVersion != protocolVersion())
        return ServerCompatibilityValidator::Reason::binaryProtocolVersionDiffers;

    return std::nullopt;
}

} // namespace

void ServerCompatibilityValidator::initialize(
    Peer localPeerType,
    Qn::SerializationFormat serializationFormat,
    DeveloperFlags developerFlags)
{
    s_localPeerType = localPeerType;
    s_serializationFormat = serializationFormat;
    s_developerFlags = developerFlags;
}

bool ServerCompatibilityValidator::isInitialized()
{
    return s_localPeerType != Peer::notDefined
        && s_serializationFormat != Qn::SerializationFormat::unsupported;
}

std::optional<ServerCompatibilityValidator::Reason> ServerCompatibilityValidator::check(
    const SiteInformation& siteInformation,
    Purpose purpose)
{
    return checkInternal(
        siteInformation.customization,
        siteInformation.protoVersion,
        siteInformation.version,
        siteInformation.cloudHost,
        purpose);
}

std::optional<ServerCompatibilityValidator::Reason> ServerCompatibilityValidator::check(
    const ModuleInformation& moduleInformation,
    Purpose purpose)
{
    return checkInternal(
        moduleInformation.customization,
        moduleInformation.protoVersion,
        moduleInformation.version,
        moduleInformation.cloudHost,
        purpose);
}

bool ServerCompatibilityValidator::isCompatible(
    const SiteInformation& siteInformation,
    Purpose purpose)
{
    return check(siteInformation, purpose) == std::nullopt;
}

bool ServerCompatibilityValidator::isCompatible(
    const ModuleInformation& moduleInformation,
    Purpose purpose)
{
    return check(moduleInformation, purpose) == std::nullopt;
}

nx::utils::SoftwareVersion ServerCompatibilityValidator::minimalVersion(Purpose purpose)
{
    return s_localPeerType == Peer::mobileClient
        ? kMinimalMobileVersion
        : kMinimalVersionByPurpose[purpose];
}

bool ServerCompatibilityValidator::isCompatibleCustomization(const QString& remoteCustomization)
{
    ensureInitialized();
    if (s_developerFlags.testFlag(DeveloperFlag::ignoreCustomization))
        return true;

    if (remoteCustomization == nx::branding::customization())
        return true;

    return s_localPeerType == Peer::mobileClient
        ? nx::branding::compatibleCustomizations().contains(remoteCustomization)
        : false;
}

} //  namespace nx::vms::common
