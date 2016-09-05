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

bool compatibleCustomization(const QString& customization, const ec2::ApiRuntimeData& localInfo)
{
    QString c1 = customization;
    QString c2 = localInfo.brand;
    if (c1.isEmpty() || c2.isEmpty())
        return true;

    if (!localInfo.peer.isMobileClient())
        return c1 == c2;

    /* For mobile clients country-local customizations must be compatible with base, e.g.
       'default' should connect to 'default_cn' and 'default_zh_CN' */
    const QChar kSeparator = L'_';
    c1 = c1.left(c1.indexOf(kSeparator));
    c2 = c2.left(c2.indexOf(kSeparator));
    return c1 == c2;
}

} // namespace

QnSoftwareVersion QnConnectionValidator::minSupportedVersion()
{
    return ::minSupportedVersion(qnRuntimeInfoManager->localInfo().data);
}

Qn::ConnectionResult QnConnectionValidator::validateConnection(
    const QnModuleInformation& info)
{
    return validateConnectionInternal(info.customization, info.protoVersion, info.version,
        info.cloudHost);
}

Qn::ConnectionResult QnConnectionValidator::validateConnection(
    const QnConnectionInfo& connectionInfo,
    ec2::ErrorCode networkError)
{
    using namespace Qn;
    if (networkError == ec2::ErrorCode::unauthorized)
        return ConnectionResult::Unauthorized;

    if (networkError == ec2::ErrorCode::temporary_unauthorized)
        return ConnectionResult::TemporaryUnauthorized;

    if (networkError != ec2::ErrorCode::ok)
        return ConnectionResult::NetworkError;

    return validateConnectionInternal(connectionInfo.brand, connectionInfo.nxClusterProtoVersion,
        connectionInfo.version, connectionInfo.cloudHost);
}

Qn::ConnectionResult QnConnectionValidator::validateConnectionInternal(
    const QString& customization,
    int protoVersion,
    const QnSoftwareVersion& version,
    const QString& cloudHost)
{
    using namespace Qn;

    if (!cloudHost.isEmpty() && cloudHost != QnAppInfo::defaultCloudHost())
        return ConnectionResult::IncompatibleInternal;

    auto localInfo = qnRuntimeInfoManager->localInfo().data;
    if (!compatibleCustomization(customization, localInfo))
        return ConnectionResult::IncompatibleInternal;

    if (!customization.isEmpty()
        && !localInfo.brand.isEmpty()
        && customization != localInfo.brand)

    if (version < minSupportedVersion())
        return ConnectionResult::IncompatibleVersion;

    if (protoVersion != QnAppInfo::ec2ProtoVersion())
        return ConnectionResult::CompatibilityMode;

    return ConnectionResult::Success;
}
