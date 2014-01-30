
#ifndef MEDIA_SERVER_MANAGER_H
#define MEDIA_SERVER_MANAGER_H

#include "nx_ec/data/mserver_data.h"
#include "nx_ec/ec_api.h"
#include "transaction/transaction.h"


namespace ec2
{
    template<class QueryProcessorType>
    class QnMediaServerManager
    :
        public AbstractMediaServerManager
    {
    public:
        QnMediaServerManager( QueryProcessorType* const queryProcessor );

        //!Implementation of QnMediaServerManager::getServers
        virtual ReqID getServers( impl::GetServersHandlerPtr handler ) override;
        //!Implementation of QnMediaServerManager::save
        virtual ReqID save( const QnMediaServerResourcePtr& resource, impl::SimpleHandlerPtr handler ) override;
        //!Implementation of QnMediaServerManager::saveServer
        virtual ReqID saveServer( const QnMediaServerResourcePtr&, impl::SaveServerHandlerPtr handler ) override;
        //!Implementation of QnMediaServerManager::remove
        virtual ReqID remove( const QnMediaServerResourcePtr& resource, impl::SimpleHandlerPtr handler ) override;

    private:
        QueryProcessorType* const m_queryProcessor;

        QnTransaction<ApiMediaServerData> prepareTransaction( ApiCommand command, const QnMediaServerResourcePtr& resource );
    };
}

#endif  //MEDIA_SERVER_MANAGER_H
