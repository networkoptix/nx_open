#pragma once 

#include <set>

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

    bool isAuthentificationRequired(nx_http::Request& request);
    void enableUnauthorizedForwarding(const QString& path);

    void setPreparedTcpSocket(std::unique_ptr<AbstractStreamServerSocket> socket);

    static std::unique_ptr<AbstractStreamServerSocket> createAndPrepareTcpSocket(
        const SocketAddress& localAddress);

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
    std::set<QString> m_unauthorizedForwardingPaths;
    std::unique_ptr<AbstractStreamServerSocket> m_preparedTcpSocket;

    void onCloudBindingStatusChanged(
        boost::optional<nx::hpm::api::SystemCredentials> cloudCredentials);
    void updateCloudConnectState(QnMutexLockerBase* const lk);
};
