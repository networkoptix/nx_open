#ifndef __CONNECTION_DIAGNOSTICS_HELPER_H__
#define __CONNECTION_DIAGNOSTICS_HELPER_H__

#include <QtCore/QObject>

#include <nx_ec/ec_api_fwd.h>

struct QnConnectionInfo;

class QnConnectionDiagnosticsHelper: public QObject
{
    Q_OBJECT
public:
    enum class Result {
        Success,
        Failure,
        Restart
    };

    /** Light check of connection validity. Returns Success if we can connect without problems, Failure otherwise. */
    static Result validateConnectionLight(const QnConnectionInfo &connectionInfo, ec2::ErrorCode errorCode);

    /** Full check of the connection. Displays message boxes, suggests to restart client, etc. */
    static Result validateConnection(const QnConnectionInfo &connectionInfo, ec2::ErrorCode errorCode, const QUrl &url, QWidget* parentWidget);
};

#endif // __CONNECTION_DIAGNOSTICS_HELPER_H__
