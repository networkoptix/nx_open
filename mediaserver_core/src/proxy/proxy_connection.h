#ifndef __PROXY_CONNECTION_H_
#define __PROXY_CONNECTION_H_

#include <QtNetwork/QHostAddress>
#include "network/universal_request_processor.h"

class QnAbstractStreamDataProvider;
class QnProxyConnectionProcessorPrivate;
struct QnRoute;

namespace ec2 {
    class TransactionMessageBusAdapter;
}

class QnProxyConnectionProcessor: public QnTCPConnectionProcessor
{
public:
    QnProxyConnectionProcessor(
        ec2::TransactionMessageBusAdapter* messageBus,
        QSharedPointer<AbstractStreamSocket> socket,
        QnHttpConnectionListener* owner);

    QnProxyConnectionProcessor(
        QnProxyConnectionProcessorPrivate* priv,
        QSharedPointer<AbstractStreamSocket> socket,
        QnHttpConnectionListener* owner);


    static void cleanupProxyInfo(nx::network::http::Request* request);

    virtual ~QnProxyConnectionProcessor();
protected:
    virtual void run() override;
private:
    bool doProxyData(
        AbstractStreamSocket* srcSocket,
        AbstractStreamSocket* dstSocket,
        char* buffer,
        int bufferSize,
        bool* outSomeBytesRead);
    static int getDefaultPortByProtocol(const QString& protocol);
    QString connectToRemoteHost(const QnRoute& route, const nx::utils::Url& url); // return new client request buffer size or -1 if error
    bool isProtocol(const QString& protocol) const;
    void doRawProxy();
    void doSmartProxy();
    bool openProxyDstConnection();
    QUrl getDefaultProxyUrl();
    bool updateClientRequest(nx::utils::Url& dstUrl, QnRoute& dstRoute);
    bool replaceAuthHeader();

    /** Returns false if socket would block in blocking mode */
    bool readSocketNonBlock(
        int* returnValue, AbstractStreamSocket* socket, void* buffer, int bufferSize);
private:
    Q_DECLARE_PRIVATE(QnProxyConnectionProcessor);
};

#endif // __PROXY_CONNECTION_H_
