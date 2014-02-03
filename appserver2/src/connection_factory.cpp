/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#include "connection_factory.h"

#include <functional>

#include <QtConcurrent>

#include "rest/ec2_query_http_handler.h"
#include "rest/ec2_update_http_handler.h"
#include "transaction/transaction.h"


namespace ec2
{
    Ec2DirectConnectionFactory::Ec2DirectConnectionFactory()
    {
    }

    Ec2DirectConnectionFactory::~Ec2DirectConnectionFactory()
    {
    }

    //!Implementation of AbstractECConnectionFactory::testConnectionAsync
    ReqID Ec2DirectConnectionFactory::testConnectionAsync( const QUrl& /*addr*/, impl::SimpleHandlerPtr handler )
    {
        QtConcurrent::run( std::bind( std::mem_fn( &impl::SimpleHandler::done ), handler, ec2::ErrorCode::ok ) );
        return 0;
    }

    //!Implementation of AbstractECConnectionFactory::connectAsync
    ReqID Ec2DirectConnectionFactory::connectAsync( const QUrl& /*addr*/, impl::ConnectHandlerPtr handler )
    {
        {
            std::lock_guard<std::mutex> lk( m_mutex );
            if( !m_connection )
                m_connection.reset( new Ec2DirectConnection( &m_queryProcessor, m_resourceFactory, m_resPool) );
        }
        QtConcurrent::run( std::bind( std::mem_fn( &impl::ConnectHandler::done ), handler, ec2::ErrorCode::ok, m_connection ) );
        return 0;
    }

    void Ec2DirectConnectionFactory::registerRestHandlers( QnRestProcessorPool* const restProcessorPool )
    {
        restProcessorPool->registerHandler(
            QString::fromLatin1("ec2/%1").arg(ApiCommand::toString(ApiCommand::getResourceTypes)),
            new QueryHttpHandler<nullptr_t, ApiResourceTypeList>(&m_queryProcessor, ApiCommand::getResourceTypes) );

        restProcessorPool->registerHandler(
            QString::fromLatin1("ec2/%1").arg(ApiCommand::toString(ApiCommand::addCamera)),
            new UpdateHttpHandler<ApiCameraData>(&m_queryProcessor) );
        restProcessorPool->registerHandler(
            QString::fromLatin1("ec2/%1").arg(ApiCommand::toString(ApiCommand::getCameras)),
            new QueryHttpHandler<QnId, ApiCameraDataList>(&m_queryProcessor, ApiCommand::getCameras) );
    }

	void Ec2DirectConnectionFactory::setResourceFactory(QSharedPointer<QnResourceFactory> factory)
	{
		m_resourceFactory = factory;
	}

    void Ec2DirectConnectionFactory::setResourcePool(QnResourcePool* pool)
    {
        m_resPool = pool;
    }
}
