#include "connection_validator.h"

#include <api/model/connection_info.h>
#include <api/global_settings.h>
#include <api/runtime_info_manager.h>
#include <common/common_module.h>
#include <common/common_module_aware.h>
#include <common/static_common_module.h>
#include <network/system_helpers.h>
#include <nx_ec/impl/ec_api_impl.h>
#include <nx/utils/software_version.h>
#include <utils/common/app_info.h>

#include <nx/network/app_info.h>
#include <nx/network/socket_global.h>
#include <nx/network/cloud/cloud_connect_controller.h>
#include <nx/vms/api/data/module_information.h>

namespace {

nx::utils::SoftwareVersion minSupportedVersion(nx::vms::api::PeerType localPeerType)
{
    if (nx::vms::api::PeerData::isMobileClient(localPeerType))
        return nx::utils::SoftwareVersion("2.5");

    if (QnAppInfo::applicationPlatform() == lit("macosx"))
        return nx::utils::SoftwareVersion("3.0");

    return nx::utils::SoftwareVersion("1.4");
}

bool isDefaultCustomization(const QString& customization)
{
    return customization == "default" || customization == "hdwitness";
}

bool isMetaCustomization(const QString& customization)
{
    return customization == "metavms";
}

bool compatibleCloudHost(const QString& cloudHost, bool isMobile)
{
    if (cloudHost.isEmpty())
        return true;

    auto activeCloud = nx::network::SocketGlobals::cloud().cloudHost();
    return cloudHost == activeCloud
        || (isMobile && nx::network::AppInfo::compatibleCloudHosts().contains(cloudHost));
}

} // namespace

nx::vms::api::SoftwareVersion QnConnectionValidator::minSupportedVersion()
{
    return ::minSupportedVersion(qnStaticCommon->localPeerType());
}

Qn::ConnectionResult QnConnectionValidator::validateConnection(
    const nx::vms::api::ModuleInformation& info)
{
    return validateConnectionInternal(
        info.brand,
        info.customization,
        info.protoVersion,
        info.version,
        info.cloudHost);
}

Qn::ConnectionResult QnConnectionValidator::validateConnection(
    const QnConnectionInfo& connectionInfo,
    ec2::ErrorCode networkError)
{
    using namespace Qn;
    if (networkError == ec2::ErrorCode::unauthorized)
        return Qn::UnauthorizedConnectionResult;

    if (networkError == ec2::ErrorCode::ldap_temporary_unauthorized)
        return Qn::LdapTemporaryUnauthorizedConnectionResult;
    else if (networkError == ec2::ErrorCode::cloud_temporary_unauthorized)
        return Qn::CloudTemporaryUnauthorizedConnectionResult;
    else if (networkError == ec2::ErrorCode::disabled_user_unauthorized)
        return Qn::DisabledUserConnectionResult;
    else if (networkError == ec2::ErrorCode::forbidden)
        return Qn::ForbiddenConnectionResult;
    else if (networkError == ec2::ErrorCode::userLockedOut)
        return Qn::UserTemporaryLockedOut;

    if (networkError != ec2::ErrorCode::ok)
        return Qn::NetworkErrorConnectionResult;

    return validateConnectionInternal(
        connectionInfo.brand,
        connectionInfo.customization,
        connectionInfo.nxClusterProtoVersion,
        connectionInfo.version,
        connectionInfo.cloudHost);
}

bool QnConnectionValidator::isCompatibleToCurrentSystem(const nx::vms::api::ModuleInformation& info,
    const QnCommonModule* commonModule)
{
    return !info.localSystemId.isNull()
        && helpers::serverBelongsToCurrentSystem(info, commonModule)
        && validateConnection(info) == Qn::SuccessConnectionResult;
}

// For mobile clients country-local customizations must be compatible with base, e.g.
// 'default' should connect to 'default_cn' and 'default_zh_CN'. The same applied to brands
// comparing, e.g. 'hdwitness' is compatible with 'hdwitness_cn' on mobile clients.
bool QnConnectionValidator::isCompatibleCustomization(
    const QString& serverCustomization,
    const QString& clientCustomization,
    bool isMobile)
{
    // In developer mode customization is empty.
    if (serverCustomization.isEmpty() || clientCustomization.isEmpty())
        return true;

    if (!isMobile)
        return serverCustomization == clientCustomization;

    if (isDefaultCustomization(clientCustomization) && isMetaCustomization(serverCustomization))
        return true;

    const QChar kSeparator = L'_';
    return serverCustomization.left(serverCustomization.indexOf(kSeparator)) == clientCustomization.left(clientCustomization.indexOf(kSeparator));
}

Qn::ConnectionResult QnConnectionValidator::validateConnectionInternal(
    const QString& brand,
    const QString& customization,
    int protoVersion,
    const nx::vms::api::SoftwareVersion& version,
    const QString& cloudHost)
{
    bool isMobile = nx::vms::api::PeerData::isMobileClient(qnStaticCommon->localPeerType());

    if (!isCompatibleCustomization(brand, qnStaticCommon->brand(), isMobile))
        return Qn::IncompatibleInternalConnectionResult;

    if (!isCompatibleCustomization(customization, qnStaticCommon->customization(), isMobile))
        return Qn::IncompatibleInternalConnectionResult;

    if (!compatibleCloudHost(cloudHost, isMobile))
        return Qn::IncompatibleCloudHostConnectionResult;

    if (version < minSupportedVersion())
        return Qn::IncompatibleVersionConnectionResult;

    // Mobile client can connect to servers with any protocol version.
    if (!isMobile && protoVersion != QnAppInfo::ec2ProtoVersion())
        return Qn::IncompatibleProtocolConnectionResult;

    // For debug purposes only
    /*
    auto appVersion = nx::vms::api::SoftwareVersion(QnAppInfo::applicationVersion());
    if (appVersion < version)
        return Qn::IncompatibleProtocolConnectionResult;*/

    return Qn::SuccessConnectionResult;
}
