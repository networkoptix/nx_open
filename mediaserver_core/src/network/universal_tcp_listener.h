#pragma once 

#include <set>

#include <network/http_connection_listener.h>
#include <nx/network/cloud/abstract_cloud_system_credentials_provider.h>
#include <nx/network/multiple_server_socket.h>
#include <nx/utils/thread/mutex.h>
#include <nx/network/http/http_mod_manager.h>

namespace nx {
namespace vms {
namespace cloud_integration {

class CloudConnectionManager;

} // namespace cloud_integration
} // namespace vms
} // namespace nx

class QnUniversalTcpListener:
    public QnHttpConnectionListener
{
public:
    QnUniversalTcpListener(
        QnCommonModule* commonModule,
        const nx::vms::cloud_integration::CloudConnectionManager& cloudConnectionManager,
        const QHostAddress& address,
        int port,
        int maxConnections,
        bool useSsl);
    ~QnUniversalTcpListener();

    void addProxySenderConnections(const nx::network::SocketAddress& proxyUrl, int size);
    nx::network::http::HttpModManager* httpModManager() const;
    virtual void applyModToRequest(nx::network::http::Request* request) override;

    bool isAuthentificationRequired(nx::network::http::Request& request);
    void enableUnauthorizedForwarding(const QString& path);

    void setPreparedTcpSockets(std::vector<std::unique_ptr<nx::network::AbstractStreamServerSocket>> sockets);

    static std::vector<std::unique_ptr<nx::network::AbstractStreamServerSocket>> 
        createAndPrepareTcpSockets(const nx::network::SocketAddress& localAddress);

protected:
    virtual QnTCPConnectionProcessor* createRequestProcessor(
        QSharedPointer<nx::network::AbstractStreamSocket> clientSocket) override;
    virtual nx::network::AbstractStreamServerSocket* createAndPrepareSocket(
        bool sslNeeded,
        const nx::network::SocketAddress& localAddress) override;
    virtual void destroyServerSocket(nx::network::AbstractStreamServerSocket* serverSocket) override;

private:
    const nx::vms::cloud_integration::CloudConnectionManager& m_cloudConnectionManager;
    nx::network::MultipleServerSocket* m_multipleServerSocket;
    std::unique_ptr<nx::network::AbstractStreamServerSocket> m_serverSocket;
    QnMutex m_mutex;
    bool m_boundToCloud;
    nx::hpm::api::SystemCredentials m_cloudCredentials;
    std::unique_ptr<nx::network::http::HttpModManager> m_httpModManager;
    //#define LISTEN_ON_UDT_SOCKET
#if defined(LISTEN_ON_UDT_SOCKET)
    std::atomic<int> m_cloudSocketIndex{1};
    std::atomic<int> m_totalListeningSockets{2};
#else
    std::atomic<int> m_cloudSocketIndex{0};
    std::atomic<int> m_totalListeningSockets{1};
#endif

    std::set<QString> m_unauthorizedForwardingPaths;
    std::vector<std::unique_ptr<nx::network::AbstractStreamServerSocket>> m_preparedTcpSockets;

    void onCloudBindingStatusChanged(
        boost::optional<nx::hpm::api::SystemCredentials> cloudCredentials);
    void updateCloudConnectState(QnMutexLockerBase* const lk);
};
