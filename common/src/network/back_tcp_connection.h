#ifndef BACK_TCP_CONNECTION_H__
#define BACK_TCP_CONNECTION_H__

#include <QUrl>
#include "utils/common/long_runnable.h"
#include "utils/network/socket.h"
#include "network/universal_request_processor.h"

class QnBackTcpConnectionPrivate;


class QnBackTcpConnection: public QnUniversalRequestProcessor
{
public:
    QnBackTcpConnection(const QUrl& serverUrl, QnTcpListener* owner);
    virtual ~QnBackTcpConnection();
protected:
    virtual void run() override;
private:
    Q_DECLARE_PRIVATE(QnBackTcpConnection);
};

#endif // BACK_TCP_CONNECTION_H__
