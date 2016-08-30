#pragma once

#include <QtCore/QObject>

#include <nx_ec/ec_api_fwd.h>

struct QnConnectionInfo;
struct QnModuleInformation;
class QnSoftwareVersion;

namespace Qn {

enum class ConnectionResult
{
    success,                    /*< Connection available. */
    networkError,               /*< Connection could not be established. */
    unauthorized,               /*< Invalid login/password. */
    temporaryUnauthorized,      /*< LDAP server is not accessible. */
    incompatibleInternal,       /*< Server has incompatible customization or cloud host. */
    incompatibleVersion,        /*< Server version is too low. */
    compatibilityMode           /*< Client should be restarted in compatibility mode.*/
};

}

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
class QnConnectionValidator: public QObject
{
    Q_OBJECT
    using base_type = QObject;
public:
    QnConnectionValidator(QObject* parent = nullptr);

    static QnSoftwareVersion minSupportedVersion();

    static Qn::ConnectionResult validateConnection(const QnModuleInformation& info);
    static Qn::ConnectionResult validateConnection(const QnConnectionInfo& connectionInfo,
        ec2::ErrorCode networkError);

protected:
    static Qn::ConnectionResult validateConnectionInternal(
        const QString& customization,
        int protoVersion,
        const QnSoftwareVersion& version,
        const QString& cloudHost);

};
