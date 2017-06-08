#include "connection_validator.h"

#include <api/model/connection_info.h>
#include <api/runtime_info_manager.h>

#include <nx_ec/impl/ec_api_impl.h>

#include <common/common_module.h>

#include <network/module_information.h>

#include <utils/common/software_version.h>
#include <utils/common/app_info.h>
#include <api/global_settings.h>
#include <network/system_helpers.h>

namespace {

QnSoftwareVersion minSupportedVersion(const ec2::ApiRuntimeData& localInfo)
{
    if (localInfo.peer.isMobileClient())
        return QnSoftwareVersion("2.5");

    if (QnAppInfo::applicationPlatform() == lit("macosx"))
        return QnSoftwareVersion("3.0");

    return QnSoftwareVersion("1.4");
}

/* For mobile clients country-local customizations must be compatible with base, e.g.
 * 'default' should connect to 'default_cn' and 'default_zh_CN'. The same applied to brands
 * comparing, e.g. 'hdwitness' is compatible with 'hdwitness_cn' on mobile clients. */
bool compatibleCustomization(const QString& c1, const QString& c2, bool isMobile)
{
    if (c1.isEmpty() || c2.isEmpty())
        return true;

    if (!isMobile)
        return c1 == c2;

    const QChar kSeparator = L'_';
    return c1.left(c1.indexOf(kSeparator)) == c2.left(c2.indexOf(kSeparator));
}

bool compatibleCloudHost(const QString& cloudHost, bool isMobile)
{
    if (cloudHost.isEmpty())
        return true;

    return cloudHost == QnAppInfo::defaultCloudHost()
        || (isMobile && QnAppInfo::compatibleCloudHosts().contains(cloudHost));
}

} // namespace

QnSoftwareVersion QnConnectionValidator::minSupportedVersion()
{
    return ::minSupportedVersion(qnRuntimeInfoManager->localInfo().data);
}

Qn::ConnectionResult QnConnectionValidator::validateConnection(
    const QnModuleInformation& info)
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
    else if (networkError == ec2::ErrorCode::forbidden)
        return Qn::ForbiddenConnectionResult;

    if (networkError != ec2::ErrorCode::ok)
        return Qn::NetworkErrorConnectionResult;

    return validateConnectionInternal(
        connectionInfo.brand,
        connectionInfo.customization,
        connectionInfo.nxClusterProtoVersion,
        connectionInfo.version,
        connectionInfo.cloudHost);
}

bool QnConnectionValidator::isCompatibleToCurrentSystem(const QnModuleInformation& info)
{
    return !info.localSystemId.isNull()
        && helpers::serverBelongsToCurrentSystem(info)
        && validateConnection(info) == Qn::SuccessConnectionResult;
}

Qn::ConnectionResult QnConnectionValidator::validateConnectionInternal(
    const QString& brand,
    const QString& customization,
    int protoVersion,
    const QnSoftwareVersion& version,
    const QString& cloudHost)
{
    auto localInfo = qnRuntimeInfoManager->localInfo().data;
    bool isMobile = localInfo.peer.isMobileClient();

    if (!compatibleCustomization(brand, localInfo.brand, isMobile))
        return Qn::IncompatibleInternalConnectionResult;

    if (!compatibleCustomization(customization, localInfo.customization, isMobile))
        return Qn::IncompatibleInternalConnectionResult;

    if (!compatibleCloudHost(cloudHost, isMobile))
        return Qn::IncompatibleCloudHostConnectionResult;

    if (version < minSupportedVersion())
        return Qn::IncompatibleVersionConnectionResult;

    // Mobile client can connect to servers with any protocol version.
    if (!isMobile && protoVersion != QnAppInfo::ec2ProtoVersion())
        return Qn::IncompatibleProtocolConnectionResult;

    return Qn::SuccessConnectionResult;
}
