
#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H

#include "nx_ec/ec_api.h"
#include "nx_ec/data/ec2_resource_data.h"
#include "transaction/transaction.h"


namespace ec2
{
    template<class QueryProcessorType>
    class QnResourceManager
    :
        public AbstractResourceManager
    {
    public:
        QnResourceManager( QueryProcessorType* const queryProcessor );

        //!Implementation of AbstractResourceManager::getResourceTypes
        virtual ReqID getResourceTypes( impl::GetResourceTypesHandlerPtr handler ) override;
        //!Implementation of AbstractResourceManager::getResources
        virtual ReqID getResources( impl::GetResourcesHandlerPtr handler ) override;
        //!Implementation of AbstractResourceManager::getResource
        virtual ReqID getResource( const QnId& id, impl::GetResourceHandlerPtr handler ) override;
        //!Implementation of AbstractResourceManager::setResourceStatus
        virtual ReqID setResourceStatus( const QnId& resourceId, QnResource::Status status, impl::SimpleHandlerPtr handler ) override;
        //!Implementation of AbstractResourceManager::getKvPairs
        virtual ReqID getKvPairs( const QnResourcePtr &resource, impl::GetKvPairsHandlerPtr handler ) override;
        //!Implementation of AbstractResourceManager::setResourceDisabled
        virtual ReqID setResourceDisabled( const QnId& resourceId, bool disabled, impl::SimpleHandlerPtr handler ) override;
        //!Implementation of AbstractResourceManager::save
        virtual ReqID save( const QnResourcePtr &resource, impl::SimpleHandlerPtr handler ) override;
        //!Implementation of AbstractResourceManager::save
        virtual ReqID save( int resourceId, const QnKvPairList& kvPairs, impl::SimpleHandlerPtr handler ) override;
        //!Implementation of AbstractResourceManager::remove
        virtual ReqID remove( const QnResourcePtr& resource, impl::SimpleHandlerPtr handler ) override;
    private:
        QnTransaction<ApiSetResourceStatusData> prepareTransaction( ApiCommand::Value cmd, const QnId& id, QnResource::Status status);
    private:
        QueryProcessorType* const m_queryProcessor;
    };
}

#endif  //RESOURCE_MANAGER_H
