
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
        virtual int getResourceTypes( impl::GetResourceTypesHandlerPtr handler ) override;
        //!Implementation of AbstractResourceManager::getResources
        virtual int getResources( impl::GetResourcesHandlerPtr handler ) override;
        //!Implementation of AbstractResourceManager::getResource
        virtual int getResource( const QnId& id, impl::GetResourceHandlerPtr handler ) override;
        //!Implementation of AbstractResourceManager::setResourceStatus
        virtual int setResourceStatus( const QnId& resourceId, QnResource::Status status, impl::SetResourceStatusHandlerPtr handler ) override;
        //!Implementation of AbstractResourceManager::getKvPairs
        virtual int getKvPairs( const QnId &resourceId, impl::GetKvPairsHandlerPtr handler ) override;
        //!Implementation of AbstractResourceManager::setResourceDisabled
        virtual int setResourceDisabled( const QnId& resourceId, bool disabled, impl::SimpleHandlerPtr handler ) override;
        //!Implementation of AbstractResourceManager::save
        virtual int save( const QnResourcePtr &resource, impl::SimpleHandlerPtr handler ) override;
        //!Implementation of AbstractResourceManager::save
        virtual int save( const QnId& resourceId, const QnKvPairList& kvPairs, impl::SaveKvPairsHandlerPtr handler ) override;
        //!Implementation of AbstractResourceManager::remove
        virtual int remove( const QnResourcePtr& resource, impl::SimpleHandlerPtr handler ) override;
    private:
        QnTransaction<ApiSetResourceStatusData> prepareTransaction( ApiCommand::Value cmd, const QnId& id, QnResource::Status status);
        QnTransaction<ApiResourceParams> prepareTransaction(ApiCommand::Value cmd, const QnId& id, const QnKvPairList& kvPairs);

    private:
        QueryProcessorType* const m_queryProcessor;
    };
}

#endif  //RESOURCE_MANAGER_H
