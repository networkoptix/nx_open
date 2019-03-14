#pragma once

#include <common/common_globals.h>
#include <nx_ec/ec_api_fwd.h>

#include <nx/vms/api/data_fwd.h>
#include <nx/vms/api/data/software_version.h>

struct QnConnectionInfo;
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
class QnConnectionValidator
{
public:
    static nx::vms::api::SoftwareVersion minSupportedVersion();

    static Qn::ConnectionResult validateConnection(const nx::vms::api::ModuleInformation& info);
    static Qn::ConnectionResult validateConnection(const QnConnectionInfo& connectionInfo,
        ec2::ErrorCode networkError);

    static bool isCompatibleToCurrentSystem(const nx::vms::api::ModuleInformation& info,
        const QnCommonModule* commonModule);

    static bool isCompatibleCustomization(
        const QString& serverCustomization,
        const QString& clientCustomization,
        bool isMobile);

protected:
    static Qn::ConnectionResult validateConnectionInternal(
        const QString& brand,
        const QString& customization,
        int protoVersion,
        const nx::vms::api::SoftwareVersion& version,
        const QString& cloudHost);
};
