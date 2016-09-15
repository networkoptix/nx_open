#include "connection_validator.h"

#include <api/model/connection_info.h>
#include <api/runtime_info_manager.h>

#include <nx_ec/impl/ec_api_impl.h>

#include <common/common_module.h>

#include <network/module_information.h>

#include <utils/common/software_version.h>
#include <utils/common/app_info.h>

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
        return ConnectionResult::Unauthorized;

    if (networkError == ec2::ErrorCode::ldap_temporary_unauthorized)
        return ConnectionResult::LdapTemporaryUnauthorized;
    else if (networkError == ec2::ErrorCode::cloud_temporary_unauthorized)
        return ConnectionResult::CloudTemporaryUnauthorized;

    if (networkError != ec2::ErrorCode::ok)
        return ConnectionResult::NetworkError;

    return validateConnectionInternal(
        connectionInfo.brand,
        connectionInfo.customization,
        connectionInfo.nxClusterProtoVersion,
        connectionInfo.version,
        connectionInfo.cloudHost);
}

bool QnConnectionValidator::isCompatibleToCurrentSystem(const QnModuleInformation& info)
{
    return info.systemName == qnCommon->localSystemName()
        && validateConnection(info) == Qn::ConnectionResult::Success;
}

Qn::ConnectionResult QnConnectionValidator::validateConnectionInternal(
    const QString& brand,
    const QString& customization,
    int protoVersion,
    const QnSoftwareVersion& version,
    const QString& cloudHost)
{
    using namespace Qn;

    if (!cloudHost.isEmpty() && cloudHost != QnAppInfo::defaultCloudHost())
        return ConnectionResult::IncompatibleInternal;

    auto localInfo = qnRuntimeInfoManager->localInfo().data;
    bool isMobile = localInfo.peer.isMobileClient();

    if (!compatibleCustomization(brand, localInfo.brand, isMobile))
        return ConnectionResult::IncompatibleInternal;

    if (!compatibleCustomization(customization, localInfo.customization, isMobile))
        return ConnectionResult::IncompatibleInternal;

    if (version < minSupportedVersion())
        return ConnectionResult::IncompatibleVersion;

    // Mobile client can connect to servers with any protocol version.
    if (!isMobile && protoVersion != QnAppInfo::ec2ProtoVersion())
        return ConnectionResult::IncompatibleProtocol;

    return ConnectionResult::Success;
}
