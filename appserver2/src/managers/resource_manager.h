
#ifndef RESOURCE_MANAGER_H
#define RESOURCE_MANAGER_H

#include "nx_ec/ec_api.h"
#include "nx_ec/data/ec2_resource_data.h"
#include "transaction/transaction.h"
#include <core/resource_management/resource_pool.h>


namespace ec2
{
    template<class QueryProcessorType>
    class QnResourceManager
    :
        public AbstractResourceManager
    {
    public:
        QnResourceManager( QueryProcessorType* const queryProcessor, const ResourceContext& resCtx );

        //!Implementation of AbstractResourceManager::getResourceTypes
        virtual int getResourceTypes( impl::GetResourceTypesHandlerPtr handler ) override;
        //!Implementation of AbstractResourceManager::setResourceStatus
        virtual int setResourceStatus( const QnId& resourceId, QnResource::Status status, impl::SetResourceStatusHandlerPtr handler ) override;
        //!Implementation of AbstractResourceManager::setResourceDisabled
        virtual int setResourceDisabled( const QnId& resourceId, bool disabled, impl::SetResourceDisabledHandlerPtr handler ) override;
        //!Implementation of AbstractResourceManager::getKvPairs
        virtual int getKvPairs( const QnId &resourceId, impl::GetKvPairsHandlerPtr handler ) override;
        //!Implementation of AbstractResourceManager::save
        virtual int save( const QnResourcePtr &resource, impl::SaveResourceHandlerPtr handler ) override;
        //!Implementation of AbstractResourceManager::save
        virtual int save( const QnId& resourceId, const QnKvPairList& kvPairs, impl::SaveKvPairsHandlerPtr handler ) override;
        //!Implementation of AbstractResourceManager::remove
        virtual int remove( const QnId& id, impl::SimpleHandlerPtr handler ) override;

        void triggerNotification( const QnTransaction<ApiResource>& tran ) {
            QnResourcePtr resource( new QnResource() );
            tran.params.toResource( resource );
            QnResourcePtr existResource = m_resCtx.pool->getResourceById(tran.params.id);
            if (existResource)
                resource->setFlags(existResource->flags());
            emit resourceChanged( resource );
        }

        void triggerNotification( const QnTransaction<ApiSetResourceStatusData>& tran ) {
            emit statusChanged( QnId(tran.params.id), tran.params.status );
        }

        void triggerNotification( const QnTransaction<ApiSetResourceDisabledData>& tran ) {
            emit disabledChanged( QnId(tran.params.id), tran.params.disabled );
        }

        void triggerNotification( const QnTransaction<ApiResourceParams>& tran ) {
            QnKvPairList outData;

            for( const ApiResourceParam& param: tran.params.params )
                outData << QnKvPair(param.name, param.value);
            emit resourceParamsChanged( tran.params.id, outData );
        }

        void triggerNotification( const QnTransaction<ApiIdData>& tran ) {
            emit resourceRemoved( tran.params.id );
        }

    private:
        QueryProcessorType* const m_queryProcessor;
        const ResourceContext& m_resCtx;

        QnTransaction<ApiSetResourceStatusData> prepareTransaction( ApiCommand::Value cmd, const QnId& id, QnResource::Status status);
        QnTransaction<ApiSetResourceDisabledData> prepareTransaction( ApiCommand::Value command, const QnId& id, bool disabled );
        QnTransaction<ApiResourceParams> prepareTransaction(ApiCommand::Value cmd, const QnId& id, const QnKvPairList& kvPairs);
        QnTransaction<ApiIdData> prepareTransaction( ApiCommand::Value cmd, const QnId& id);
        QnTransaction<ApiResource> prepareTransaction( ApiCommand::Value command, const QnResourcePtr& resource );
    };
}

#endif  //RESOURCE_MANAGER_H
