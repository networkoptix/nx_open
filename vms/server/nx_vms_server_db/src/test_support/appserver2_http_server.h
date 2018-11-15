#pragma once

#include <functional>

#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtNetwork/QHostAddress>

#include <nx/network/abstract_socket.h>
#include <nx/network/http/http_types.h>

#include <nx/vms/api/types_fwd.h>
#include <nx/vms/auth/abstract_nonce_provider.h>
#include <nx/vms/auth/abstract_user_data_provider.h>

#include <api/common_message_processor.h>
#include <network/http_connection_listener.h>
#include <nx_ec/ec_api.h>
#include <rest/server/json_rest_result.h>
#include <test_support/resource/test_resource_factory.h>

class QnCommonModule;
class QnTCPConnectionProcessor;

namespace ec2 {

using ProcessorHandler = std::function<
    nx::network::http::StatusCode::Value(
        const nx::network::http::Request&,
        QnHttpConnectionListener* owner,
        QnJsonRestResult* result)>;

class JsonConnectionProcessor:
    public QnTCPConnectionProcessor
{
public:
    JsonConnectionProcessor(
        ProcessorHandler handler,
        std::unique_ptr<nx::network::AbstractStreamSocket> socket,
        QnHttpConnectionListener* owner);

    virtual void run() override;

private:
    ProcessorHandler m_handler;
};

//-------------------------------------------------------------------------------------------------

class QnSimpleHttpConnectionListener:
    public QnHttpConnectionListener
{
public:
    QnSimpleHttpConnectionListener(
        QnCommonModule* commonModule,
        const QHostAddress& address,
        int port,
        int maxConnections,
        bool useSsl);

    ~QnSimpleHttpConnectionListener();

    bool needAuth(const nx::network::http::Request& request) const;
    void disableAuthForPath(const QString& path);
    void setAuthenticator(
        nx::vms::auth::AbstractUserDataProvider* userDataProvider,
        nx::vms::auth::AbstractNonceProvider* nonceProvider);

protected:
    virtual QnTCPConnectionProcessor* createRequestProcessor(
        std::unique_ptr<nx::network::AbstractStreamSocket> clientSocket) override;

private:
    QSet<QString> m_disableAuthPrefixes;
    nx::vms::auth::AbstractUserDataProvider* m_userDataProvider = nullptr;
    nx::vms::auth::AbstractNonceProvider* m_nonceProvider = nullptr;
};

//-------------------------------------------------------------------------------------------------

class Appserver2MessageProcessor:
    public QnCommonMessageProcessor
{
    using base_type = QnCommonMessageProcessor;
public:
    Appserver2MessageProcessor(QObject* parent);

protected:
    virtual void onResourceStatusChanged(
        const QnResourcePtr& /*resource*/,
        Qn::ResourceStatus /*status*/,
        ec2::NotificationSource /*source*/) override;

    virtual QnResourceFactory* getResourceFactory() const override;

    virtual bool canRemoveResource(const QnUuid& id) override;

    virtual void removeResourceIgnored(const QnUuid& resourceId) override;

    virtual void updateResource(
        const QnResourcePtr& resource,
        ec2::NotificationSource /*source*/) override;

    virtual void handleRemotePeerFound(QnUuid peer, nx::vms::api::PeerType peerType) override;
    virtual void handleRemotePeerLost(QnUuid peer, nx::vms::api::PeerType peerType) override;

protected:
    std::unique_ptr<nx::TestResourceFactory> m_factory;
    QSet<QnUuid> m_delayedOnlineStatus;
};

} // namespace ec2
