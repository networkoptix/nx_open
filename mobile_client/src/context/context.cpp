#include "context.h"

#include <nx_ec/ec_api.h>
#include <nx_ec/ec2_lib.h>

#include <api/app_server_connection.h>

#include <core/resource_management/resource_pool.h>

#include <plugins/resource/server_camera/server_camera_factory.h>

#include <client/client_message_processor.h>

Context::Context(QObject *parent):
    base_type(parent)
{
    store<ec2::AbstractECConnectionFactory>(getConnectionFactory());

    ec2::ResourceContext resCtx(QnServerCameraFactory::instance(), qnResPool, qnResTypePool);

    QScopedPointer<ec2::AbstractECConnectionFactory> ec2ConnectionFactory(getConnectionFactory());
    m_connectionFactory->setContext( resCtx );
    QnAppServerConnectionFactory::setEC2ConnectionFactory( ec2ConnectionFactory.data() );

    QScopedPointer<QnClientMessageProcessor> clientMessageProcessor(new QnClientMessageProcessor());
}

Context::~Context() {
    return;
}



