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
        Failure,
        Restart
    };

    struct TestConnectionResult {
        Result result;
        QString details;
        int helpTopicId;
    };

    /** Light check of connection validity. Returns Success if we can connect without problems, Failure otherwise. */
    static Result validateConnectionLight(
        const QString &brand,
        const QnSoftwareVersion &version,
        int protoVersion,
        const QList<QnCompatibilityItem> &compatibilityItems = QList<QnCompatibilityItem>());

    /** Light check of connection validity. Returns Success if we can connect without problems, Failure otherwise. */
    static Result validateConnectionLight(const QnConnectionInfo &connectionInfo, ec2::ErrorCode errorCode);

    /** Full check of the connection. Displays message boxes, suggests to restart client, etc. */
    static Result validateConnection(const QnConnectionInfo &connectionInfo, ec2::ErrorCode errorCode, const QUrl &url, QWidget* parentWidget);

    //TODO: #GDM think about refactoring
    /** Another check of connection, used from 'Test connection' dialog. */
    static TestConnectionResult validateConnectionTest(const QnConnectionInfo &connectionInfo, ec2::ErrorCode errorCode);
};

#endif // __CONNECTION_DIAGNOSTICS_HELPER_H__
