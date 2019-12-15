#pragma once

#include <QtNetwork/QHostAddress>
#include <network/tcp_connection_processor.h>
#include <nx/vms/network/reverse_connection_manager.h>

class QnAbstractStreamDataProvider;
struct QnRoute;

namespace ec2 {
    class TransactionMessageBusAdapter;
}

namespace nx {
namespace vms {
namespace network {

class ProxyConnectionProcessorPrivate;

class ProxyConnectionProcessor: public QnTCPConnectionProcessor
{
public:
    ProxyConnectionProcessor(
        nx::vms::network::ReverseConnectionManager* reverseConnectionManager,
        std::unique_ptr<nx::network::AbstractStreamSocket> socket,
        QnHttpConnectionListener* owner);

    static void cleanupProxyInfo(nx::network::http::Request* request);

    virtual ~ProxyConnectionProcessor();

    static bool isProxyNeeded(QnCommonModule* commonModule, const nx::network::http::Request& request);
    static bool isStandardProxyNeeded(QnCommonModule* commonModule, const nx::network::http::Request& request);
    static bool isCloudRequest(const nx::network::http::Request& request);
    static bool isProxyForCamera(QnCommonModule* commonModule, const nx::network::http::Request& request);
protected:
    virtual void run() override;
private:

    bool doProxyData(
        nx::network::AbstractStreamSocket* srcSocket,
        nx::network::AbstractStreamSocket* dstSocket,
        char* buffer,
        int bufferSize,
        qint64* outBytesRead);
    QString connectToRemoteHost(const QnRoute& route, const nx::utils::Url& url); // return new client request buffer size or -1 if error
    void doRawProxy();
    void doSmartProxy();
    bool openProxyDstConnection();
    bool updateClientRequest(nx::utils::Url& dstUrl, QnRoute& dstRoute);
    void fixServerUrlSchemeSecurity(nx::utils::Url& dstUrl, QnRoute& dstRoute);
    bool replaceAuthHeader();

    /** Returns false if socket would block in blocking mode */
    bool readSocketNonBlock(
        int* returnValue, nx::network::AbstractStreamSocket* socket, void* buffer, int bufferSize);
    void doProxyRequest();
private:
    Q_DECLARE_PRIVATE(nx::vms::network::ProxyConnectionProcessor);
};

} // namespace network
} // namespace vms
} // namespace nx
