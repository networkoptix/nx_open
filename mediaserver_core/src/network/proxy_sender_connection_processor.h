#ifndef BACK_TCP_CONNECTION_H__
#define BACK_TCP_CONNECTION_H__

#include <QUrl>
#include "utils/common/long_runnable.h"
#include <nx/network/socket.h>
#include "network/universal_request_processor.h"

class QnProxySenderConnectionPrivate;


class QnProxySenderConnection: public QnUniversalRequestProcessor
{
public:
    QnProxySenderConnection(const SocketAddress& proxyServerUrl, const QnUuid& guid,
                            QnUniversalTcpListener* owner);

    virtual ~QnProxySenderConnection();
protected:
    virtual void run() override;
private:
    QByteArray readProxyResponse();
    void doDelay();
    int sendRequest(const QByteArray& data);
    QByteArray makeProxyRequest(const QnUuid& serverUuid, const QUrl& url) const;
private:
    Q_DECLARE_PRIVATE(QnProxySenderConnection);
};

#endif // BACK_TCP_CONNECTION_H__
