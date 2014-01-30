/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#include "connection_factory.h"

#include <functional>

#include <QtConcurrent>

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
    ReqID Ec2DirectConnectionFactory::testConnectionAsync( const ECAddress& /*addr*/, impl::SimpleHandlerPtr handler )
    {
        QtConcurrent::run( std::bind( std::mem_fn( &impl::SimpleHandler::done ), handler, ec2::ErrorCode::ok ) );
        return 0;
    }

    //!Implementation of AbstractECConnectionFactory::connectAsync
    ReqID Ec2DirectConnectionFactory::connectAsync( const ECAddress& /*addr*/, impl::ConnectHandlerPtr handler )
    {
        {
            std::lock_guard<std::mutex> lk( m_mutex );
            if( !m_connection )
                m_connection.reset( new Ec2DirectConnection(m_resourceFactory) );
        }
        QtConcurrent::run( std::bind( std::mem_fn( &impl::ConnectHandler::done ), handler, ec2::ErrorCode::ok, m_connection ) );
        return 0;
    }

	void Ec2DirectConnectionFactory::setResourceFactory(QSharedPointer<QnResourceFactory> factory)
	{
		m_resourceFactory = factory;
	}
}

