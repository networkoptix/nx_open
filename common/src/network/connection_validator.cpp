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
#include <common/common_module_aware.h>

namespace {

QnSoftwareVersion minSupportedVersion(Qn::PeerType localPeerType)
{
    if (ec2::ApiPeerData::isMobileClient(localPeerType))
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

QnConnectionValidator::QnConnectionValidator(Qn::PeerType localPeerType,
    const QnModuleInformation& localModule):
    m_peerType(localPeerType),
    m_moduleInformation(localModule)
{

}

QnSoftwareVersion QnConnectionValidator::minSupportedVersion() const
{
    return ::minSupportedVersion(m_peerType);
}

Qn::ConnectionResult QnConnectionValidator::validateConnection(
    const QnModuleInformation& info) const
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
    ec2::ErrorCode networkError) const
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

bool QnConnectionValidator::isCompatibleToCurrentSystem(const QnModuleInformation& info,
    const QnCommonModule* commonModule) const
{
    return !info.localSystemId.isNull()
        && helpers::serverBelongsToCurrentSystem(info, commonModule)
        && validateConnection(info) == Qn::SuccessConnectionResult;
}

Qn::ConnectionResult QnConnectionValidator::validateConnectionInternal(
    const QString& brand,
    const QString& customization,
    int protoVersion,
    const QnSoftwareVersion& version,
    const QString& cloudHost) const
{
    bool isMobile = ec2::ApiPeerData::isMobileClient(m_peerType);

    if (!compatibleCustomization(brand, m_moduleInformation.brand, isMobile))
        return Qn::IncompatibleInternalConnectionResult;

    if (!compatibleCustomization(customization, m_moduleInformation.customization, isMobile))
        return Qn::IncompatibleInternalConnectionResult;

    if (!cloudHost.isEmpty() && cloudHost != QnAppInfo::defaultCloudHost())
        return Qn::IncompatibleCloudHostConnectionResult;

    if (version < minSupportedVersion())
        return Qn::IncompatibleVersionConnectionResult;

    // Mobile client can connect to servers with any protocol version.
    if (!isMobile && protoVersion != QnAppInfo::ec2ProtoVersion())
        return Qn::IncompatibleProtocolConnectionResult;

    return Qn::SuccessConnectionResult;
}
