#ifndef __UNIVERSAL_TCP_LISTENER_H__
#define __UNIVERSAL_TCP_LISTENER_H__

#include <network/http_connection_listener.h>
#include <nx/network/cloud/abstract_cloud_system_credentials_provider.h>
#include <nx/network/multiple_server_socket.h>
#include <nx/utils/thread/mutex.h>


class CloudConnectionManager;

class QnUniversalTcpListener
:
    public QnHttpConnectionListener
{
public:
    QnUniversalTcpListener(
        const CloudConnectionManager& cloudConnectionManager,
        const QHostAddress& address,
        int port,
        int maxConnections,
        bool useSsl);
    ~QnUniversalTcpListener();
        
    void addProxySenderConnections(const SocketAddress& proxyUrl, int size);

protected:
    virtual QnTCPConnectionProcessor* createRequestProcessor(
        QSharedPointer<AbstractStreamSocket> clientSocket) override;
    virtual AbstractStreamServerSocket* createAndPrepareSocket(
        bool sslNeeded,
        const SocketAddress& localAddress) override;
    virtual void destroyServerSocket(AbstractStreamServerSocket* serverSocket) override;
    
private:
    const CloudConnectionManager& m_cloudConnectionManager;
    nx::network::MultipleServerSocket* m_multipleServerSocket;
    std::unique_ptr<AbstractStreamServerSocket> m_serverSocket;
    QnMutex m_mutex;
    bool m_boundToCloud;
    nx::hpm::api::SystemCredentials m_cloudCredentials;

    void onCloudBindingStatusChanged(
        boost::optional<nx::hpm::api::SystemCredentials> cloudCredentials);
    void updateCloudConnectState(QnMutexLockerBase* const lk);
};

#endif  //__UNIVERSAL_TCP_LISTENER_H__
