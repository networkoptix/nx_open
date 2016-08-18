#ifndef __CONNECTION_DIAGNOSTICS_HELPER_H__
#define __CONNECTION_DIAGNOSTICS_HELPER_H__

#include <QtCore/QObject>

#include <nx_ec/ec_api_fwd.h>
#include <api/model/compatibility_item.h>

struct QnConnectionInfo;
class QnSoftwareVersion;

class QnConnectionDiagnosticsHelper: public QObject
{
    Q_OBJECT
public:
    enum class Result {
        Success,
        RestartRequested,
        IncompatibleBrand,
        IncompatibleVersion,
        IncompatibleProtocol,
        Unauthorized,
        TemporaryUnauthorized,
        ServerError
    };

    static QnSoftwareVersion minSupportedVersion();

#ifdef _DEBUG
    static QString resultToString(Result value);
#endif //  _DEBUG

    struct TestConnectionResult {
        Result result;
        QString details;
        int helpTopicId;
    };

    /** Light check of connection validity. Returns Success if we can
        connect without problems, Failure otherwise. */
    static Result validateConnectionLight(
        const QString &brand, int protoVersion, const QnSoftwareVersion& version);

    /** Light check of connection validity. Returns Success if we can connect without problems, Failure otherwise. */
    static Result validateConnectionLight(const QnConnectionInfo &connectionInfo, ec2::ErrorCode errorCode);

    /** Full check of the connection. Displays message boxes, suggests to restart client, etc. */
    static Result validateConnection(const QnConnectionInfo &connectionInfo, ec2::ErrorCode errorCode, const QUrl &url, QWidget* parentWidget);

    //TODO: #GDM think about refactoring
    /** Another check of connection, used from 'Test connection' dialog. */
    static TestConnectionResult validateConnectionTest(const QnConnectionInfo &connectionInfo, ec2::ErrorCode errorCode);

private:
    //TODO: #GDM move all duplicating strings here
    enum class ErrorStrings {
        ContactAdministrator,
        UnableConnect
    };

    static QString strings(ErrorStrings id);

};

#endif // __CONNECTION_DIAGNOSTICS_HELPER_H__
