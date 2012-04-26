#ifndef __PROXY_CONNECTION_H_
#define __PROXY_CONNECTION_H_

#include <QHostAddress>
#include "utils/network/tcp_connection_processor.h"
#include "utils/common/base.h"

class QnAbstractStreamDataProvider;

class QnProxyConnectionProcessor: public QnTCPConnectionProcessor
{
public:
    QnProxyConnectionProcessor(TCPSocket* socket, QnTcpListener* owner);
    virtual ~QnProxyConnectionProcessor();
    bool isLiveDP(QnAbstractStreamDataProvider* dp);
protected:
    virtual void run() override;
private:
    static bool doProxyData(fd_set* read_set, TCPSocket* srcSocket, TCPSocket* dstSocket, char* buffer, int bufferSize);
    static int getDefaultPortByProtocol(const QString& protocol);
private:
    QN_DECLARE_PRIVATE_DERIVED(QnProxyConnectionProcessor);
};

#endif // __PROXY_CONNECTION_H_
