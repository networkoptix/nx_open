#pragma once

#include <set>

#include <network/http_connection_listener.h>
#include <nx/network/cloud/abstract_cloud_system_credentials_provider.h>
#include <nx/network/multiple_server_socket.h>
#include <nx/utils/thread/mutex.h>
#include <nx/network/http/http_mod_manager.h>
#include <nx/vms/server/authenticator.h>

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
    using base_type = QnHttpConnectionListener;
public:

    QnUniversalTcpListener(
        QnCommonModule* commonModule,
        const QHostAddress& address,
        int port,
        int maxConnections,
        bool useSsl);
    ~QnUniversalTcpListener();

    void setupAuthorizer(
        TimeBasedNonceProvider* timeBasedNonceProvider,
        nx::vms::cloud_integration::CloudManagerGroup& cloudManagerGroup);

    void setCloudConnectionManager(
        const nx::vms::cloud_integration::CloudConnectionManager& cloudConnectionManager);

    void addProxySenderConnections(const nx::network::SocketAddress& proxyUrl, int size);
    nx::network::http::HttpModManager* httpModManager() const;
    virtual void applyModToRequest(nx::network::http::Request* request) override;

    nx::vms::server::Authenticator* authenticator() const;
    static nx::vms::server::Authenticator* authenticator(const QnTcpListener* listener);

    bool isAuthentificationRequired(nx::network::http::Request& request);
    void enableUnauthorizedForwarding(const QString& path);

    static std::vector<std::unique_ptr<nx::network::AbstractStreamServerSocket>>
        createAndPrepareTcpSockets(const nx::network::SocketAddress& localAddress);
    virtual void stop() override;
protected:
    virtual QnTCPConnectionProcessor* createRequestProcessor(
        std::unique_ptr<nx::network::AbstractStreamSocket> clientSocket) override;
    virtual std::unique_ptr<nx::network::AbstractStreamServerSocket> createAndPrepareSocket(
        bool sslNeeded,
        const nx::network::SocketAddress& localAddress) override;
    virtual void destroyServerSocket() override;
private:
    std::unique_ptr<nx::vms::server::Authenticator> m_authenticator;
    nx::network::MultipleServerSocket* m_multipleServerSocket = nullptr;
    QnMutex m_mutex;
    bool m_boundToCloud;
    nx::hpm::api::SystemCredentials m_cloudCredentials;
    std::unique_ptr<nx::network::http::HttpModManager> m_httpModManager;
    std::atomic<int> m_cloudSocketIndex{0};

    std::set<QString> m_unauthorizedForwardingPaths;

    void onCloudBindingStatusChanged(
        std::optional<nx::hpm::api::SystemCredentials> cloudCredentials);
    void updateCloudConnectState(QnMutexLockerBase* const lk);
};
