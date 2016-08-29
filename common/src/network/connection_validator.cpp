#include "connection_validator.h"

#include <api/model/connection_info.h>
#include <api/runtime_info_manager.h>

#include <nx_ec/impl/ec_api_impl.h>

#include <common/common_module.h>

#include <network/module_information.h>

#include <utils/common/software_version.h>
#include <utils/common/app_info.h>

namespace {

QnSoftwareVersion minSupportedVersion()
{
#if defined(Q_OS_MACX)
    return QnSoftwareVersion("3.0");
#else
    return QnSoftwareVersion("1.4");
#endif
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

}

QnConnectionValidator::QnConnectionValidator(QObject* parent):
    base_type(parent)
{
}

QnSoftwareVersion QnConnectionValidator::minSupportedVersion()
{
    return ::minSupportedVersion();
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
        return ConnectionResult::unauthorized;

    if (networkError == ec2::ErrorCode::temporary_unauthorized)
        return ConnectionResult::temporaryUnauthorized;

    if (networkError != ec2::ErrorCode::ok)
        return ConnectionResult::networkError;

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
        return ConnectionResult::incompatibleInternal;

    auto localInfo = qnRuntimeInfoManager->localInfo().data;
    if (!compatibleCustomization(customization, localInfo))
        return ConnectionResult::incompatibleInternal;

    if (!customization.isEmpty()
        && !localInfo.brand.isEmpty()
        && customization != localInfo.brand)

    if (version < minSupportedVersion())
        return ConnectionResult::incompatibleVersion;

    if (!version.isCompatible(QnSoftwareVersion(QnAppInfo::engineVersion())))
        return ConnectionResult::compatibilityMode;

    if (protoVersion != QnAppInfo::ec2ProtoVersion())
        return ConnectionResult::updateRequested;

    return ConnectionResult::success;
}
