#include "context.h"

#include <nx_ec/ec_api.h>
#include <nx_ec/ec2_lib.h>

//#include <

Context::Context(QObject *parent):
    base_type(parent)
{
    m_connectionFactory.reset(getConnectionFactory());

    ec2::ResourceContext resCtx(&QnServerCameraFactory::instance(), qnResPool, qnResTypePool);

    //QScopedPointer<ec2::AbstractECConnectionFactory> ec2ConnectionFactory(getConnectionFactory());
    m_connectionFactory->setContext( resCtx );
    QnAppServerConnectionFactory::setEC2ConnectionFactory( ec2ConnectionFactory.get() );

    QScopedPointer<QnClientMessageProcessor> clientMessageProcessor(new QnClientMessageProcessor());

}

virtual Context::~Context() {

}



