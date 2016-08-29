#pragma once

#include <QtCore/QObject>

#include <nx_ec/ec_api_fwd.h>

#include <network/connection_validator.h>

class QnConnectionDiagnosticsHelper: public QnConnectionValidator
{
    Q_OBJECT
    using base_type = QnConnectionValidator;
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
        const QUrl &url,
        QWidget* parentWidget);

    //TODO: #GDM think about refactoring
    /** Another check of connection, used from 'Test connection' dialog. */
    static TestConnectionResult validateConnectionTest(
        const QnConnectionInfo &connectionInfo,
        ec2::ErrorCode errorCode);

private:
    //TODO: #GDM move all duplicating strings here
    enum class ErrorStrings
    {
        ContactAdministrator,
        UnableConnect
    };

    static QString strings(ErrorStrings id);

};
