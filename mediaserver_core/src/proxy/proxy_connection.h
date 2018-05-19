#pragma once

#include <QtNetwork/QHostAddress>
#include "network/universal_request_processor.h"
#include <nx/mediaserver/server_module_aware.h>
#include <nx/vms/network/reverse_connection_manager.h>

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
        nx::vms::network::ReverseConnectionManager* reverseConnectionManager,
        QSharedPointer<nx::network::AbstractStreamSocket> socket,
        QnHttpConnectionListener* owner);

    static void cleanupProxyInfo(nx::network::http::Request* request);

    virtual ~QnProxyConnectionProcessor();
protected:
    virtual void run() override;
private:
    bool doProxyData(
        nx::network::AbstractStreamSocket* srcSocket,
        nx::network::AbstractStreamSocket* dstSocket,
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
        int* returnValue, nx::network::AbstractStreamSocket* socket, void* buffer, int bufferSize);
private:
    Q_DECLARE_PRIVATE(QnProxyConnectionProcessor);
};
