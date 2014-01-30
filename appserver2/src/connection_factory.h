/**********************************************************
* 21 jan 2014
* a.kolesnikov
***********************************************************/

#ifndef EC2_CONNECTION_FACTORY_H
#define EC2_CONNECTION_FACTORY_H

#include <memory>
#include <mutex>

#include "ec2_connection.h"
#include "nx_ec/ec_api.h"
#include "server_query_processor.h"


namespace ec2
{
    class Ec2DirectConnectionFactory
    :
        public AbstractECConnectionFactory
    {
    public:
        Ec2DirectConnectionFactory();
        virtual ~Ec2DirectConnectionFactory();

        //!Implementation of AbstractECConnectionFactory::testConnectionAsync
        virtual ReqID testConnectionAsync( const QUrl& addr, impl::SimpleHandlerPtr handler ) override;
        //!Implementation of AbstractECConnectionFactory::connectAsync
        virtual ReqID connectAsync( const QUrl& addr, impl::ConnectHandlerPtr handler ) override;

        virtual void registerRestHandlers( QnRestProcessorPool* const restProcessorPool ) override;
		virtual void setResourceFactory(QSharedPointer<QnResourceFactory>) override;

    private:
        ServerQueryProcessor m_queryProcessor;
        AbstractECConnectionPtr m_connection;
		QSharedPointer<QnResourceFactory> m_resourceFactory;
        std::mutex m_mutex;
    };
}

#endif  //EC2_CONNECTION_FACTORY_H
