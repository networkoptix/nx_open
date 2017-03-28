#pragma once

#include <nx_ec/ec_api_fwd.h>
#include <common/common_module_aware.h>

struct QnConnectionInfo;
struct QnModuleInformation;
class QnSoftwareVersion;
class QnCommonModule;

/**
* Helper class to diagnose connection possibility.
* First, network connection is checked.
* Then we are checking login and password. For slow LDAP connections there is special
* 'TemporaryUnauthorized' option.
* For correct credentials there are possible outcomes:
*   * If server has different cloud host - is it incompatible
*   * If server version is lower than 1.4 (3.0 for mac), it is incompatible
*   * If server has other customization (and no one is in devmode) - is is incompatible
*   * If server version is the same as clients, but protocol version differs - update requested
*   * Otherwise, compatibility mode is enabled.
*   * Internal 'ServerError' is for all other cases.
*/
class QnConnectionValidator: public QnCommonModuleAware
{
public:
    QnConnectionValidator(QnCommonModule* commonModule);

    QnSoftwareVersion minSupportedVersion() const;

    Qn::ConnectionResult validateConnection(const QnModuleInformation& info) const;
    Qn::ConnectionResult validateConnection(const QnConnectionInfo& connectionInfo,
        ec2::ErrorCode networkError) const;

    bool isCompatibleToCurrentSystem(const QnModuleInformation& info) const;

protected:
    Qn::ConnectionResult validateConnectionInternal(
        const QString& brand,
        const QString& customization,
        int protoVersion,
        const QnSoftwareVersion& version,
        const QString& cloudHost) const;
};
