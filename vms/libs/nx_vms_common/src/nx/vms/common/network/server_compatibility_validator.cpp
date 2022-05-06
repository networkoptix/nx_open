// Copyright 2018-present Network Optix, Inc. Licensed under MPL 2.0: www.mozilla.org/MPL/2.0/

#include "server_compatibility_validator.h"

#include <nx/branding.h>
#include <nx/build_info.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/network/socket_global.h>
#include <nx/vms/api/data/module_information.h>
#include <nx/vms/api/data/system_information.h>
#include <nx/vms/api/protocol_version.h>
#include <nx_ec/ec_api_common.h>

using namespace nx::vms::api;

namespace nx::vms::common {

using Peer = ServerCompatibilityValidator::Peer;
using Protocol = ServerCompatibilityValidator::Protocol;
using Purpose = ServerCompatibilityValidator::Purpose;
using DeveloperFlag = ServerCompatibilityValidator::DeveloperFlag;
using DeveloperFlags = ServerCompatibilityValidator::DeveloperFlags;

namespace {

static Peer s_localPeerType = Peer::undefined;
static Protocol s_localProtocolType = Protocol::undefined;
static DeveloperFlags s_developerFlags = {};

static const QMap<Peer, Protocol> kDefaultProtocolByPeer{
    {Peer::undefined, Protocol::undefined},
    {Peer::server, Protocol::ubjson},
    {Peer::mobileClient, Protocol::json},
    {Peer::desktopClient, Protocol::ubjson},
};

/** The lowest version of the Server to which Client can try to connect. */
static const nx::utils::SoftwareVersion kMinimalConnectVersion(4, 0);

/** The lowest version of the Server which can be merged to existing System */
static const nx::utils::SoftwareVersion kMinimalMergeVersion(5, 0);

static const QMap<Purpose, nx::utils::SoftwareVersion> kMinimalVersionByPurpose{
    {Purpose::connect, kMinimalConnectVersion},
    {Purpose::merge, kMinimalMergeVersion},
};

bool isClient(Peer peerType)
{
    return peerType == Peer::desktopClient || peerType == Peer::mobileClient;
}

void ensureInitialized()
{
    NX_ASSERT(s_localPeerType != Peer::undefined && s_localProtocolType != Protocol::undefined);
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
    const SoftwareVersion& version,
    const QString& cloudHost,
    ServerCompatibilityValidator::Purpose purpose)
{
    ensureInitialized();

    if (version < kMinimalVersionByPurpose[purpose])
        return ServerCompatibilityValidator::Reason::versionIsTooLow;

    if (!ServerCompatibilityValidator::isCompatibleCustomization(customization))
        return ServerCompatibilityValidator::Reason::customizationDiffers;

    if (!compatibleCloudHost(cloudHost))
        return ServerCompatibilityValidator::Reason::cloudHostDiffers;

    // Binary protocol requires the same protocol version.
    const bool ensureProtocolVersion = (s_localProtocolType == Protocol::ubjson);

    if (ensureProtocolVersion && protoVersion != protocolVersion())
        return ServerCompatibilityValidator::Reason::binaryProtocolVersionDiffers;

    return std::nullopt;
}

} // namespace

void ServerCompatibilityValidator::initialize(
    Peer localPeerType,
    Protocol connectionProtocol,
    DeveloperFlags developerFlags)
{
    s_localPeerType = localPeerType;
    s_localProtocolType = connectionProtocol == Protocol::autoDetect
        ? kDefaultProtocolByPeer[localPeerType]
        : connectionProtocol;
    s_developerFlags = developerFlags;
}

std::optional<ServerCompatibilityValidator::Reason> ServerCompatibilityValidator::check(
    const SystemInformation& systemInformation,
    Purpose purpose)
{
    return checkInternal(
        systemInformation.customization,
        systemInformation.protoVersion,
        systemInformation.version,
        systemInformation.cloudHost,
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
    const SystemInformation& systemInformation,
    Purpose purpose)
{
    return check(systemInformation, purpose) == std::nullopt;
}

bool ServerCompatibilityValidator::isCompatible(
    const ModuleInformation& moduleInformation,
    Purpose purpose)
{
    return check(moduleInformation, purpose) == std::nullopt;
}

nx::utils::SoftwareVersion ServerCompatibilityValidator::minimalVersion(Purpose purpose)
{
    return kMinimalVersionByPurpose[purpose];
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
