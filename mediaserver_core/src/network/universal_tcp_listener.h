#ifndef __UNIVERSAL_TCP_LISTENER_H__
#define __UNIVERSAL_TCP_LISTENER_H__

#include <network/http_connection_listener.h>
#include <nx/network/cloud/abstract_cloud_system_credentials_provider.h>
#include <nx/network/multiple_server_socket.h>
#include <nx/utils/thread/mutex.h>
#include <nx/network/http/http_mod_manager.h>


class CloudConnectionManager;

class QnUniversalTcpListener
:
    public QnHttpConnectionListener
{
public:
    QnUniversalTcpListener(
        QnCommonModule* commonModule,
        const CloudConnectionManager& cloudConnectionManager,
        const QHostAddress& address,
        int port,
        int maxConnections,
        bool useSsl);
    ~QnUniversalTcpListener();

    void addProxySenderConnections(const SocketAddress& proxyUrl, int size);
    nx_http::HttpModManager* httpModManager() const;
    virtual void applyModToRequest(nx_http::Request* request) override;
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
    std::unique_ptr<nx_http::HttpModManager> m_httpModManager;
    //#define LISTEN_ON_UDT_SOCKET
#if defined(LISTEN_ON_UDT_SOCKET)
    std::atomic<int> m_cloudSocketIndex{1};
    std::atomic<int> m_totalListeningSockets{2};
#else
    std::atomic<int> m_cloudSocketIndex{0};
    std::atomic<int> m_totalListeningSockets{1};
#endif


    void onCloudBindingStatusChanged(
        boost::optional<nx::hpm::api::SystemCredentials> cloudCredentials);
    void updateCloudConnectState(QnMutexLockerBase* const lk);

    AbstractStreamServerSocket* addServerSocketToMultipleSocket(
        const SocketAddress& localAddress,
        nx::network::MultipleServerSocket* multipleServerSocket,
        int ipVersion);
};

#endif  //__UNIVERSAL_TCP_LISTENER_H__
