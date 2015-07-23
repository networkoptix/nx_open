#ifndef __PROXY_CONNECTION_H_
#define __PROXY_CONNECTION_H_

#include <QHostAddress>
#include "network/universal_request_processor.h"

class QnAbstractStreamDataProvider;
class QnProxyConnectionProcessorPrivate;
struct QnRoute;

class QnProxyConnectionProcessor: public QnTCPConnectionProcessor
{
public:
    QnProxyConnectionProcessor(
            QSharedPointer<AbstractStreamSocket> socket, QnUniversalTcpListener* owner);

    QnProxyConnectionProcessor(
            QnProxyConnectionProcessorPrivate* priv, QSharedPointer<AbstractStreamSocket> socket,
            QnUniversalTcpListener* owner);

    virtual ~QnProxyConnectionProcessor();
protected:
    virtual void run() override;
	virtual void pleaseStop() override;
private:
    static bool doProxyData(AbstractStreamSocket* srcSocket, AbstractStreamSocket* dstSocket, char* buffer, int bufferSize);
    static int getDefaultPortByProtocol(const QString& protocol);
    QString connectToRemoteHost(const QnRoute& route, const QUrl& url); // return new client request buffer size or -1 if error
    bool isProtocol(const QString& protocol) const;
    void doRawProxy();
    void doSmartProxy();
    bool openProxyDstConnection();
    QUrl getDefaultProxyUrl();
    bool updateClientRequest(QUrl& dstUrl, QnRoute& dstRoute);
private:
    Q_DECLARE_PRIVATE(QnProxyConnectionProcessor);
};

#endif // __PROXY_CONNECTION_H_
