#ifndef __PROXY_CONNECTION_H_
#define __PROXY_CONNECTION_H_

#include <QHostAddress>
#include "utils/network/tcp_connection_processor.h"

class QnAbstractStreamDataProvider;
class QnProxyConnectionProcessorPrivate;

class QnProxyConnectionProcessor: public QnTCPConnectionProcessor
{
public:
    QnProxyConnectionProcessor(QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* owner);
    QnProxyConnectionProcessor(QnProxyConnectionProcessorPrivate* priv, QSharedPointer<AbstractStreamSocket> socket, QnTcpListener* owner);

    virtual ~QnProxyConnectionProcessor();
protected:
    virtual void run() override;
	virtual void pleaseStop() override;
private:
    static bool doProxyData(fd_set* read_set, AbstractStreamSocket* srcSocket, AbstractStreamSocket* dstSocket, char* buffer, int bufferSize);
    static int getDefaultPortByProtocol(const QString& protocol);
    QString connectToRemoteHost(const QString& guid, const QUrl& url, int port); // return new client request buffer size or -1 if error
    bool isProtocol(const QString& protocol) const;
    void doRawProxy();
    void doSmartProxy();
    bool isSameDstAddr();
    QString getServerGuid();
    bool openProxyDstConnection();
    QUrl getDefaultProxyUrl();
private:
    Q_DECLARE_PRIVATE(QnProxyConnectionProcessor);
};

#endif // __PROXY_CONNECTION_H_
