#pragma once

#include <QtCore/QObject>

#include <nx_ec/ec_api_fwd.h>

#include <network/connection_validator.h>

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
        QWidget* parentWidget);

    //TODO: #GDM think about refactoring
    /** Another check of connection, used from 'Test connection' dialog. */
    static TestConnectionResult validateConnectionTest(
        const QnConnectionInfo &connectionInfo,
        ec2::ErrorCode errorCode);


    static void failedRestartClientMessage(QWidget* parent);

private:
    static bool getInstalledVersions(QList<QnSoftwareVersion>* versions);
    static Qn::ConnectionResult handleApplauncherError(QWidget* parentWidget);

    static QString getDiffVersionsText();
    static QString getDiffVersionsExtra(
        const QString& clientVersion,
        const QString& serverVersion);
    static QString getDiffVersionsFullText(
        const QString& clientVersion,
        const QString& serverVersion);

    static QString getDiffVersionFullExtras(
        const QString& clientVersion,
        const QString& serverVersion,
        const QString& extraText);

    static void showValidateConnectionErrorMessage(
        QWidget* parentWidget,
        Qn::ConnectionResult result,
        const QnConnectionInfo& connectionInfo);

    static QString ldapServerTimeoutMessage();

    static Qn::ConnectionResult handleCompatibilityMode(
        const QnConnectionInfo &connectionInfo,
        QWidget* parentWidget);

    //TODO: #GDM move all duplicating strings here
    enum class ErrorStrings
    {
        ContactAdministrator,
        UnableConnect,
        CloudIsNotReady
    };

    static QString getErrorString(ErrorStrings id);
    static QString getErrorDescription(
        Qn::ConnectionResult result, const QnConnectionInfo& connectionInfo);
};
