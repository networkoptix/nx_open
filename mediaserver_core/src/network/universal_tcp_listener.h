#pragma once 

#include <set>

#include <network/http_connection_listener.h>
#include <nx/network/cloud/abstract_cloud_system_credentials_provider.h>
#include <nx/network/multiple_server_socket.h>
#include <nx/utils/thread/mutex.h>
#include <nx/network/http/http_mod_manager.h>
#include <nx/mediaserver/authenticator.h>

namespace nx {
namespace vms {
namespace cloud_integration {

class CloudManagerGroup;
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
        TimeBasedNonceProvider* timeBasedNonceProvider,
        nx::vms::cloud_integration::CloudManagerGroup* cloudConnectionGroup,
        const QHostAddress& address,
        int port,
        int maxConnections,
        bool useSsl);
    ~QnUniversalTcpListener();

    void addProxySenderConnections(const nx::network::SocketAddress& proxyUrl, int size);
    nx::network::http::HttpModManager* httpModManager() const;
    virtual void applyModToRequest(nx::network::http::Request* request) override;

    nx::mediaserver::Authenticator* authenticator() const;
    static nx::mediaserver::Authenticator* authorizer(const QnTcpListener* listener);

    bool isAuthentificationRequired(nx::network::http::Request& request);
    void enableUnauthorizedForwarding(const QString& path);

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
    mutable nx::mediaserver::Authenticator m_authorizer;
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

    void onCloudBindingStatusChanged(
        boost::optional<nx::hpm::api::SystemCredentials> cloudCredentials);
    void updateCloudConnectState(QnMutexLockerBase* const lk);
};
