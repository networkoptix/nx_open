#pragma once

#include <QtCore/QObject>

#include <network/connection_validator.h>
#include <nx_ec/ec_api_fwd.h>

#include <nx/utils/software_version.h>

namespace nx::vms::ap { struct SoftwareVersion; }

class QnConnectionDiagnosticsHelper: public QObject
{
    Q_OBJECT
    using base_type = QObject;

public:
    QnConnectionDiagnosticsHelper(QObject* parent = nullptr);

    struct TestConnectionResult
    {
        Qn::ConnectionResult result;
        QString details;
        int helpTopicId;
    };

    /** Full check of the connection. Displays message boxes, suggests to restart client, etc. */
    static Qn::ConnectionResult validateConnection(
        const QnConnectionInfo &connectionInfo,
        ec2::ErrorCode errorCode,
        QWidget* parentWidget,
        const nx::vms::api::SoftwareVersion& engineVersion);

    // TODO: #GDM think about refactoring
    /** Another check of connection, used from 'Test connection' dialog. */
    static TestConnectionResult validateConnectionTest(
        const QnConnectionInfo &connectionInfo,
        ec2::ErrorCode errorCode,
        const nx::vms::api::SoftwareVersion& engineVersion);

    static void failedRestartClientMessage(QWidget* parent);

private:
    static bool getInstalledVersions(
        const nx::vms::api::SoftwareVersion& engineVersion,
        QList<nx::utils::SoftwareVersion>* versions);
    static Qn::ConnectionResult handleApplauncherError(QWidget* parentWidget);

    static QString getDiffVersionsText();
    static QString getDiffVersionsExtra(
        const QString& clientVersion,
        const QString& serverVersion);
    static QString getDiffVersionsFullText(
        const QString& clientVersion,
        const QString& serverVersion);

    static QString getDiffVersionFullExtras(
        const QnConnectionInfo& serverInfo,
        const QString& extraText,
        const nx::vms::api::SoftwareVersion& engineVersion);

    static void showValidateConnectionErrorMessage(
        QWidget* parentWidget,
        Qn::ConnectionResult result,
        const QnConnectionInfo& connectionInfo,
        const nx::vms::api::SoftwareVersion& engineVersion);

    static QString ldapServerTimeoutMessage();

    static Qn::ConnectionResult handleCompatibilityMode(
        const QnConnectionInfo &connectionInfo,
        QWidget* parentWidget,
        const nx::vms::api::SoftwareVersion& engineVersion);

    // TODO: #GDM move all duplicating strings here
    enum class ErrorStrings
    {
        ContactAdministrator,
        UnableConnect,
        CloudIsNotReady
    };

    static QString getErrorString(ErrorStrings id);
    static QString getErrorDescription(
        Qn::ConnectionResult result, const QnConnectionInfo& connectionInfo,
        const nx::vms::api::SoftwareVersion& engineVersion);
};
