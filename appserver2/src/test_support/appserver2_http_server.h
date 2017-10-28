#pragma once

#include <functional>

#include <QtCore/QObject>
#include <QtCore/QSet>
#include <QtCore/QString>
#include <QtNetwork/QHostAddress>

#include <nx/network/abstract_socket.h>
#include <nx/network/http/http_types.h>

#include <api/common_message_processor.h>
#include <network/http_connection_listener.h>
#include <nx_ec/ec_api.h>
#include <rest/server/json_rest_result.h>
#include <test_support/resource/test_resource_factory.h>

class QnCommonModule;
class QnTCPConnectionProcessor;

namespace ec2 {

using ProcessorHandler = std::function<
    nx_http::StatusCode::Value(
        const nx_http::Request&,
        QnHttpConnectionListener* owner,
        QnJsonRestResult* result)>;

class JsonConnectionProcessor:
    public QnTCPConnectionProcessor
{
public:
    JsonConnectionProcessor(
        ProcessorHandler handler,
        QSharedPointer<AbstractStreamSocket> socket,
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

    bool needAuth(const nx_http::Request& request) const;
    void disableAuthForPath(const QString& path);

protected:
    virtual QnTCPConnectionProcessor* createRequestProcessor(
        QSharedPointer<AbstractStreamSocket> clientSocket) override;

private:
    QSet<QString> m_disableAuthPrefixes;
};

//-------------------------------------------------------------------------------------------------

class Appserver2MessageProcessor:
    public QnCommonMessageProcessor
{
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

protected:
    std::unique_ptr<nx::TestResourceFactory> m_factory;
};

} // namespace ec2
