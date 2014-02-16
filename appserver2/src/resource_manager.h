
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
        QnResourceManager( QueryProcessorType* const queryProcessor, const ResourceContext& resCtx );

        //!Implementation of AbstractResourceManager::getResourceTypes
        virtual int getResourceTypes( impl::GetResourceTypesHandlerPtr handler ) override;
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
        virtual int remove( const QnId& id, impl::SimpleHandlerPtr handler ) override;

        template<class T> void triggerNotification( const QnTransaction<T>& tran ) {
            static_assert( false, "Specify QnResourceManager::triggerNotification<>, please" );
        }

        template<> void triggerNotification<ApiResourceData>( const QnTransaction<ApiResourceData>& tran ) {
            QnResourcePtr resource = m_resCtx.resFactory->createResource(
                tran.params.typeId,
                tran.params.toHashMap() );
            tran.params.toResource( resource );
            emit resourceChanged( resource );
        }

        template<> void triggerNotification<ApiSetResourceStatusData>( const QnTransaction<ApiSetResourceStatusData>& tran ) {
            emit statusChanged( QnId(tran.params.id), tran.params.status );
        }

        template<> void triggerNotification<ApiResourceParams>( const QnTransaction<ApiResourceParams>& tran ) {
            QnKvPairList outData;

            qint32 resourceId = 0;
            for( const ApiResourceParam& param: tran.params )
            {
                outData << QnKvPair(param.name, param.value);
                resourceId = param.resourceId;
            }
            emit resourceParamsChanged( QnId(resourceId), outData );
        }

        template<> void triggerNotification<ApiIdData>( const QnTransaction<ApiIdData>& tran ) {
            emit resourceRemoved( tran.params.id );
        }

    private:
        QueryProcessorType* const m_queryProcessor;
        const ResourceContext& m_resCtx;

        QnTransaction<ApiSetResourceStatusData> prepareTransaction( ApiCommand::Value cmd, const QnId& id, QnResource::Status status);
        QnTransaction<ApiResourceParams> prepareTransaction(ApiCommand::Value cmd, const QnId& id, const QnKvPairList& kvPairs);
    };
}

#endif  //RESOURCE_MANAGER_H
