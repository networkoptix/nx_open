#include "context.h"

#include <nx_ec/ec_api.h>

Context::Context(QObject *parent):
    base_type(parent)
{
    QScopedPointer<ec2::AbstractECConnectionFactory> ec2ConnectionFactory(getConnectionFactory());
    ec2::ResourceContext resCtx(&QnServerCameraFactory::instance(), qnResPool, qnResTypePool);
    ec2ConnectionFactory->setContext( resCtx );
    QnAppServerConnectionFactory::setEC2ConnectionFactory( ec2ConnectionFactory.get() );

    QScopedPointer<QnClientMessageProcessor> clientMessageProcessor(new QnClientMessageProcessor());

}

virtual Context::~Context() {

}



